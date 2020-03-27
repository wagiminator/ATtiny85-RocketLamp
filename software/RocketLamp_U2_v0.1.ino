// Rocket Lamp
//
// This is the code for the 2nd ATtiny85 for controlling the
// (soundreactive) NeoPixels. It is controlled by the 1st ATtiny
// by 1-wire interface.
//
//                       ATtiny85
//                        +-\/-+
// RESET -- A0 (D5) PB5  1|    |8  Vcc
// RTX ---- A3 (D3) PB3  2|    |7  PB2 (D2) A1 --- SENSOR2
// MIC ---- A2 (D4) PB4  3|    |6  PB1 (D1) ------ SENSOR1
//                  GND  4|    |5  PB0 (D0) ------ NEOPIXEL
//                        +----+    
//
// Clockspeed 8 MHz internal
//
// 2019/2020 by Stefan Wagner


// Libraries
#include <Adafruit_NeoPixel.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// Pins
#define NEOPIN    0   // neopixel
#define RTXPIN    3   // data line from 1st ATtiny
#define SOUNDPIN  A2  // analog sound sensor

// TRX Codes
#define PWRTOGGLE     1
#define PWRON         2
#define PWROFF        3
#define MODENEXT      4
#define MODEPREV      5
#define BRIGHTUP      6
#define BRIGHTDOWN    7

// data transmit parameters
#define TOGGLEMICROS  320

// Number of NeoPixels
#define NUMPIXELS 30

// Behavior
#define PULSELENGTH 15
#define PULSEDELAY  40
#define FLAMEDELAY  1
#define RAINBOWLENGTH 13
#define SAMPLEWINDOW 30

// Init Neopixel library
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS + 1, NEOPIN, NEO_GRB + NEO_KHZ800);

// Variables
uint8_t brightness = 127;
uint8_t showmode = 0;
bool keepgoing;



void setup() {
  // setup pin change interrupt
  PCMSK = bit (RTXPIN);                       // turn on interrupt on RTX pin
  GIMSK = 0;                                  // but disable pin change interrupt by now
  
  // start the NeoPixel library.
  pixels.begin();
  pixels.setBrightness(brightness);
}


void loop() {

  keepgoing = true;
  
  switch (showmode) {
    case 0: rainbowmode(); break;
    case 1: soundmode(); break;
    case 2: pulsemode(); break;
    case 3: flamemode(); break;
  }
}


void checkRTX() {

  uint8_t code = 0;
  uint32_t startmicros, micros1, micros2, micros3, micros4;
  
  if (digitalRead(RTXPIN) == LOW) {
    while (digitalRead(RTXPIN) == LOW);   startmicros = micros();
    while (digitalRead(RTXPIN) == HIGH);  micros1 = micros();
    while (digitalRead(RTXPIN) == LOW);   micros2 = micros();
    while (digitalRead(RTXPIN) == HIGH);  micros3 = micros();
    while (digitalRead(RTXPIN) == LOW);   micros4 = micros();

    if ((micros1 - startmicros) > TOGGLEMICROS) code |= 1;
    if ((micros2 - micros1) > TOGGLEMICROS) code |= 2;
    if ((micros3 - micros2) > TOGGLEMICROS) code |= 4;
    if ((micros4 - micros3) > TOGGLEMICROS) code |= 8;

    switch (code) {
      case MODENEXT:  
        if (showmode < 3) showmode+=1; else showmode=0;
        keepgoing = false;
        break;

      case MODEPREV:
        if (showmode > 0) showmode-=1; else showmode=3;
        keepgoing = false;
        break;

      case BRIGHTUP:
        if (brightness < 192) {
          brightness+=64;
          pixels.setBrightness(brightness);
        }
        break;

      case BRIGHTDOWN:
        if (brightness > 126) {
          brightness-=64;
          pixels.setBrightness(brightness);
        }
        break;

      case PWROFF:
        gotosleep();
        break;

      case PWRTOGGLE:
        gotosleep();
        break;
    }
  }
}


void gotosleep() {
  pixels.clear();
  pixels.show();
  keepgoing = false;

  do {
    // go to sleep
    // wait for wake-up call by 1st ATtiny
    set_sleep_mode (SLEEP_MODE_PWR_DOWN); // set sleep mode to power down
    GIMSK = bit (PCIE);                   // enable pin change interrupts
    bitSet (GIFR, PCIF);                  // clear any outstanding interrupts
    power_all_disable ();                 // power off ADC, Timer 0 and 1, serial interface
    noInterrupts ();                      // timed sequence coming up
    sleep_enable ();                      // ready to sleep
    interrupts ();                        // interrupts are required now
    sleep_cpu ();                         // sleep              
    sleep_disable ();                     // precaution
    power_all_enable ();                  // power everything back on
    GIMSK = 0;                            // disable pin change interrupt
  } while (digitalRead(RTXPIN) == HIGH);  // check if it was a wake up call
    
  // wait for RTX to go high again
  while (digitalRead(RTXPIN) == LOW);
}


void rainbowmode() {
  uint8_t  redtargetspeed = 45;
  uint8_t  redspeed = 45;
  uint16_t redpos = 0;
  uint32_t redmillis = 0;
  uint8_t  greentargetspeed = 30;
  uint8_t  greenspeed = 30;
  uint16_t greenpos = 0;
  uint32_t greenmillis = 0;
  uint8_t  bluetargetspeed = 20;
  uint8_t  bluespeed = 20;
  uint16_t bluepos = 0;
  uint32_t bluemillis = 0;
  uint8_t  factor = 256 / RAINBOWLENGTH;
  pixels.clear();
  pixels.setPixelColor(NUMPIXELS, 255, 255, 255);

  while (keepgoing) {
    if (millis() - redmillis > redspeed) {
      if (redspeed == redtargetspeed) redtargetspeed = random (85, 100);
      if (redspeed > redtargetspeed) redspeed-=1;
      if (redspeed < redtargetspeed) redspeed+=1;
      if (redpos == 0) redpos = NUMPIXELS;
      redpos-=1;

      uint16_t pixelpos = redpos;
      for (int i=0; i<RAINBOWLENGTH; i++) {
        uint32_t fullcolor = pixels.getPixelColor (pixelpos);
        uint8_t green = (uint8_t)(fullcolor >>  8);
        uint8_t blue = (uint8_t)fullcolor;
        pixels.setPixelColor(pixelpos, pixels.Color(i*factor, green, blue));
        pixelpos+=1;
        if (pixelpos == NUMPIXELS) pixelpos = 0;
      }
      
      for (int i=0; i<RAINBOWLENGTH; i++) {
        uint32_t fullcolor = pixels.getPixelColor (pixelpos);
        uint8_t green = (uint8_t)(fullcolor >>  8);
        uint8_t blue = (uint8_t)fullcolor;
        pixels.setPixelColor(pixelpos, pixels.Color(255-i*factor, green, blue));
        pixelpos+=1;
        if (pixelpos == NUMPIXELS) pixelpos = 0;
      }

      pixels.show();
      redmillis = millis();
    }
    
    if (millis() - greenmillis > greenspeed) {
      if (greenspeed == greentargetspeed) greentargetspeed = random (70, 85);
      if (greenspeed > greentargetspeed) greenspeed-=1;
      if (greenspeed < greentargetspeed) greenspeed+=1;
      if (greenpos == 0) greenpos = NUMPIXELS;
      greenpos-=1;

      uint16_t pixelpos = greenpos;
      for (int i=0; i<RAINBOWLENGTH; i++) {
        uint32_t fullcolor = pixels.getPixelColor (pixelpos);
        uint8_t red = (uint8_t)(fullcolor >>  16);
        uint8_t blue = (uint8_t)fullcolor;
        pixels.setPixelColor(pixelpos, pixels.Color(red, i*factor, blue));
        pixelpos+=1;
        if (pixelpos == NUMPIXELS) pixelpos = 0;
      }
      
      for (int i=0; i<RAINBOWLENGTH; i++) {
        uint32_t fullcolor = pixels.getPixelColor (pixelpos);
        uint8_t red = (uint8_t)(fullcolor >>  16);
        uint8_t blue = (uint8_t)fullcolor;
        pixels.setPixelColor(pixelpos, pixels.Color(red, 255-i*factor, blue));
        pixelpos+=1;
        if (pixelpos == NUMPIXELS) pixelpos = 0;
      }

      pixels.show();
      greenmillis = millis();
    }

    if (millis() - bluemillis > bluespeed) {
      if (bluespeed == bluetargetspeed) bluetargetspeed = random (55, 70);
      if (bluespeed > bluetargetspeed) bluespeed-=1;
      if (bluespeed < bluetargetspeed) bluespeed+=1;
      if (bluepos == 0) bluepos = NUMPIXELS;
      bluepos-=1;

      uint16_t pixelpos = bluepos;
      for (int i=0; i<RAINBOWLENGTH; i++) {
        uint32_t fullcolor = pixels.getPixelColor (pixelpos);
        uint8_t red = (uint8_t)(fullcolor >>  16);
        uint8_t green = (uint8_t)(fullcolor >>  8);
        pixels.setPixelColor(pixelpos, pixels.Color(red, green, i*factor));
        pixelpos+=1;
        if (pixelpos == NUMPIXELS) pixelpos = 0;
      }
      
      for (int i=0; i<RAINBOWLENGTH; i++) {
        uint32_t fullcolor = pixels.getPixelColor (pixelpos);
        uint8_t red = (uint8_t)(fullcolor >>  16);
        uint8_t green = (uint8_t)(fullcolor >>  8);
        pixels.setPixelColor(pixelpos, pixels.Color(red, green, 255-i*factor));
        pixelpos+=1;
        if (pixelpos == NUMPIXELS) pixelpos = 0;
      }

      pixels.show();
      bluemillis = millis();
    }

    checkRTX();   
  }
}


void flamemode() {
  uint16_t flamelength;
  uint32_t lastmillis = 0;
  pixels.clear();
  pixels.setPixelColor(NUMPIXELS, 255, 255, 255);
  
  while (keepgoing) {
    //if ((millis() - lastmillis) > FLAMEDELAY) {
      for(int i=0; i<NUMPIXELS; i++) {
        uint32_t fullcolor = pixels.getPixelColor (i);
        uint8_t red = (uint8_t)(fullcolor >>  16);
        uint8_t green = (uint8_t)(fullcolor >>  8);
        uint8_t blue = (uint8_t)fullcolor;
        if (red > i+7) red-=(i+8); else red = 0;
        if (green > i+7) green-=(i+8); else green = 0;
        if (blue > i+7) blue-=(i+8); else blue = 0;
        pixels.setPixelColor(i, pixels.Color(red, green, blue));
      }
      flamelength = random (0, NUMPIXELS) * random (0, NUMPIXELS) / NUMPIXELS;
      uint8_t redfactor = 64 / flamelength;
      uint8_t greenfactor = 128 / flamelength;
      uint8_t bluefactor = 128 / flamelength;
      for (int i=0; i<flamelength; i++) {
        pixels.setPixelColor(NUMPIXELS-1-i, 255, 255-i*greenfactor, 128-i*bluefactor);
      }
      pixels.show();
      lastmillis = millis();
  //  }
  checkRTX();
  }
}



void pulsemode() {  
  for(int i=0; i<NUMPIXELS; i++) pixels.setPixelColor(i, pixels.Color(255,130,55));
  pixels.show();
  uint8_t factor = 70 / (PULSELENGTH - 1);
  uint16_t pulsepos = NUMPIXELS - 1;
  uint32_t lastmillis = 0;
  uint8_t rocketLED;
  
  while (keepgoing) {
    if ((millis() - lastmillis) > PULSEDELAY) {
      uint16_t pixelpos = pulsepos;
      for (int i=0; i<PULSELENGTH; i++) {
        pixels.setPixelColor(pixelpos, pixels.Color(255,255 - i * factor, 255 - i * factor * 3));
        pixelpos+=1;
        if (pixelpos == NUMPIXELS) pixelpos = 0;
      }
      rocketLED = 255 - (2 * pulsepos);
      pixels.setPixelColor(NUMPIXELS, rocketLED, rocketLED, rocketLED);
      pixels.show();
      if (pulsepos == 0) pulsepos = NUMPIXELS;
      pulsepos-=1;
      lastmillis = millis();
    }
    checkRTX();
  }  
}



void soundmode() {
  uint32_t startmillis;
  uint16_t sample, signalmax, signalmin, peak2peak;
  uint16_t volume;
  uint16_t volumemax = 0;
  uint16_t volumemin = 1023;
  uint8_t rocketLED;
  uint8_t factor = 255 / (NUMPIXELS - 1);

  while (keepgoing) {

    // create short-term sample
    signalmax = 0; signalmin = 1024;
    startmillis = millis ();
    while (millis() - startmillis < SAMPLEWINDOW) {
      sample = analogRead(SOUNDPIN);
      if (sample > signalmax) signalmax = sample;
      if (sample < signalmin) signalmin = sample;
    }

    // check for IR signals
    checkRTX();

    // create long-term sample
    peak2peak = signalmax - signalmin;
    if (peak2peak > volumemax) volumemax = peak2peak;
    if (peak2peak < volumemin) volumemin = peak2peak;
    volume = map (peak2peak, volumemin, volumemax, 0, NUMPIXELS);
    rocketLED = map (peak2peak, volumemin, volumemax, 128, 255);

    // update the neopixels
    pixels.clear();
    for(int i=0; i<volume; i++) pixels.setPixelColor(NUMPIXELS-1-i, pixels.Color(255,0,i * factor));
    pixels.setPixelColor(NUMPIXELS, rocketLED, rocketLED, rocketLED);
    pixels.show();

    // slowly reset long-term sample
    if (volumemin < (1023 - NUMPIXELS)) volumemin++;
    if (volumemax > (volumemin + NUMPIXELS)) volumemax--;

    // check for IR signals
    checkRTX();
  } 
}

// pin change interrupt service routine
EMPTY_INTERRUPT (PCINT0_vect);          // nothing to be done here, just wake up from sleep
