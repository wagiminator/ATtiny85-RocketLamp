// Rocket Lamp
//
// This is the code for the 1st ATtiny85 for handling the
// IR receiver and transmitting the code via 1-wire-interface
// to the 2nd ATtiny which controls the NeoPixels.
//
//                       ATtiny85
//                        +-\/-+
// RESET -- A0 (D5) PB5  1|    |8  Vcc
// TRX ---- A3 (D3) PB3  2|    |7  PB2 (D2) A1 --- I2C SCL
// LDR ---- A2 (D4) PB4  3|    |6  PB1 (D1) ------ IR RECEIVER
//                  GND  4|    |5  PB0 (D0) ------ I2C SDA
//                        +----+    
//
// Clockspeed 8 MHz internal
//
// 2019/2020 by Stefan Wagner


// Libraries
#include <tiny_IRremote.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// Pins
#define IRPIN         1  // IR receiver
#define TRXPIN        3  // data line to 2nd ATtiny
#define LDRPIN       A2  // LDR light sensor

// IR codes
#define IRPWRTOGGLE   0xFF38C7
#define IRPWRON       0xFF807F
#define IRPWROFF      0xFF906F
#define IRMODENEXT    0xFF5AA5
#define IRMODEPREV    0xFF10EF
#define IRBRIGHTUP    0xFF18E7
#define IRBRIGHTDOWN  0xFF4AB5

// data transmit codes
#define PWRTOGGLE     1
#define PWRON         2
#define PWROFF        3
#define MODENEXT      4
#define MODEPREV      5
#define BRIGHTUP      6
#define BRIGHTDOWN    7

// data transmit parameters
#define INITMILLIS    50
#define BITLOWMICROS  100
#define BITHIGHMICROS 500

// init IRremote library
IRrecv irrecv(IRPIN);
decode_results results;

// some variables
uint32_t keyarray[] {0, IRPWRTOGGLE, IRPWRON, IRPWROFF, IRMODENEXT, IRMODEPREV, IRBRIGHTUP, IRBRIGHTDOWN};
uint32_t keypressed = 0;
uint32_t lastkey = 0;


void setup() {
  // init data line and set it to input
  bitClear(DDRB,  TRXPIN);                    // TRX pin to input (= high by 10k pull-up)
  bitClear(PORTB, TRXPIN);                    // will go low, if TRX pin is set to output

  // setup pin change interrupt
  PCMSK = bit (IRPIN);                        // turn on interrupt on IR receiver pin
  GIMSK = 0;                                  // but disable pin change interrupt by now

  // start the IR receiver
  irrecv.enableIRIn();
}


void loop() {
  if (irrecv.decode(&results)) {
    // handle IR received signal
    keypressed = results.value;
    if (keypressed == 0xFFFFFFFF) keypressed = lastkey;  
    for (uint8_t i=1; i<8; i++) {
      if (keypressed == keyarray[i]) sendhalfbyte(i);
    }
    
    if ((keypressed == IRPWRTOGGLE) || (keypressed == IRPWROFF)) gotosleep();   

    lastkey = keypressed; keypressed = 0;
    irrecv.resume();
  }
}


void gotosleep() { 
  // sleep until IR remote power on button is pressed
  keypressed = 0; irrecv.resume();
  do {
    sleep();
    if (irrecv.decode(&results)) {
      keypressed = results.value;
      irrecv.resume();
    }
  } while ((keypressed != IRPWRTOGGLE) && (keypressed != IRPWRON));
  keypressed = 0;
  
  // wake up 2nd ATtiny
  bitSet  (DDRB, TRXPIN); delay (INITMILLIS);
  bitClear(DDRB, TRXPIN); delay (200); 
}


void sleep() {
  // Go to sleep when lamp is off in order to save energy.
  // Wake up again when IR receiver triggers pin change interrupt.
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);
  GIMSK = bit (PCIE);     // enable pin change interrupts
  bitSet (GIFR, PCIF);    // clear any outstanding interrupts
  power_all_disable ();   // power off ADC, Timer 0 and 1, serial interface
  noInterrupts ();        // timed sequence coming up
  sleep_enable ();        // ready to sleep
  interrupts ();          // interrupts are required now
  sleep_cpu ();           // sleep               
  sleep_disable ();       // precaution
  power_all_enable ();    // power everything back on
  GIMSK = 0;              // disable pin change interrupt
  delay(100);             // wait for IR signal to be decoded
}


void sendhalfbyte(uint8_t code) {
  uint16_t wait1, wait2, wait3, wait4;

  // calculate signal timing
  if ((code & 1) == 1) wait1 = BITHIGHMICROS; else wait1 = BITLOWMICROS;
  if ((code & 2) == 2) wait2 = BITHIGHMICROS; else wait2 = BITLOWMICROS;
  if ((code & 4) == 4) wait3 = BITHIGHMICROS; else wait3 = BITLOWMICROS;
  if ((code & 8) == 8) wait4 = BITHIGHMICROS; else wait4 = BITLOWMICROS;

  // bitbanging the signal
  bitSet  (DDRB, TRXPIN); delay (INITMILLIS);  
  bitClear(DDRB, TRXPIN); delayMicroseconds(wait1);
  bitSet  (DDRB, TRXPIN); delayMicroseconds(wait2);
  bitClear(DDRB, TRXPIN); delayMicroseconds(wait3);
  bitSet  (DDRB, TRXPIN); delayMicroseconds(wait4);
  bitClear(DDRB, TRXPIN); delay (INITMILLIS); 
}


// pin change interrupt service routine
EMPTY_INTERRUPT (PCINT0_vect);          // nothing to be done here, just wake up from sleep
