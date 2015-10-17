/**************************************************************
 * Blynk is a platform with iOS and Android apps to control
 * Arduino, Raspberry Pi and the likes over the Internet.
 * You can easily build graphic interfaces for all your
 * projects by simply dragging and dropping widgets.
 *
 *   Downloads, docs, tutorials: http://www.blynk.cc
 *   Blynk community:            http://community.blynk.cc
 *   Social networks:            http://www.fb.com/blynkapp
 *                               http://twitter.com/blynk_app
 *
 * Blynk library is licensed under MIT license
 * This example code is in public domain.
 *
 **************************************************************
 * This example runs directly on ESP8266 chip.
 *
 * You need to install this for ESP8266 development:
 *   https://github.com/esp8266/Arduino
 *
 * Change WiFi ssid, pass, and Blynk auth token to run :)
 *
 **************************************************************/

#include <ESP8266WiFi.h>
#include <Ticker.h>


// Register
// --------
// register bit masks
const byte switchMask     = B10000000;
const byte rgbMask        = B01110000;
const byte ledColMask     = B00001111;

// control pins
const int dataPin         = 4;
const int clockPin        = 5;
const int latchPin        = 15;


// Button
// ------
// const byte btnGndPins[4]  = {(0x1 << 4), (0x1 << 5), (0x1 << 6), (0x1 << 7)}


// Options
// -------


// Init
// ----
unsigned int rgbs[4][3]   = {{220, 255, 0},{0, 255, 0},{0, 0, 255},{100, 0, 100}};
unsigned long was[4]      = {0, 0, 0, 0};
bool btns[4]              = {false, false, false, false};

volatile int column;

Ticker cycleTicker1;

// Run
// ---
void setup()
{
  // register pins
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);

  digitalWrite(dataPin, LOW);
  digitalWrite(clockPin, LOW);
  digitalWrite(latchPin, LOW);

  // reset register
  bytelatch(0x0);
  
  // start step timer
  cycleTicker1.attach_ms(1, cycle);
}

void loop()
{
  // nothing to do (yet)
}

// Functions
// ---------

void cycle()
{
  unsigned long next = micros() + 990;
  while(micros() < next) {
    column = (column + 1) % 4;
    step(column);
  }
}

/**
 * @brief Set the state (high, low) of each rgb pin for the current step
 * @details Effectively software pwm emulation (bit-banging). The state of each
 * pin (high/low) is calculated in order to achieve an approximation of the
 * desired workload (0-255) across multiple column cycles.
 * The SN74HC595 doesn't support PWM out (aka "intensity"), but it is plenty
 * fast enough to cycle a few RGB leds.
 */
void step(int col)
{
  byte data = ~(0x1 << (col + 4));
  bool on;
  int i;
  
  // mark start of column cycle
  unsigned long now = micros();

  // turn on if:
  //  workload greater than 0
  //  256µs clock, modulo workload, is less than or equal to half the workload,
  //    plus the between column cycles (to compensate for time the led was off)
  for(int i = 0; i < 3; i++) {
    on = (rgbs[col][i] > 0) && ((now % 256) % (rgbs[col][i] + 1) <= (((rgbs[col][i] + 1) / 2) + (now - was[col])));
    if (on) {
      data |= 0x1 << (i + 1);
    } else {
      data &= ~(0x1 << (i + 1));
    }
  }

  bytelatch(data);
  delayMicroseconds(24);
  // record end to calculate time between column cycles
  was[col] = micros();
}

/**
 * @brief Shift byte data into register and toggle data clock (latch)
 * 
 * @param data byte to shift in (least significant bit first)
 */
void bytelatch(byte data)
{
  // make sure clock is low
  digitalWrite(clockPin, LOW);
  // shift data into position
  shiftOut(dataPin, clockPin, MSBFIRST, data);
  // output in parallel
  blip(latchPin, true, 0);
}

/**
 * @brief Momentary toggle with a short delay
 * @details Toggle a pin high/low, then hold for a number of microseconds before
 * setting pin to the opposite. Useful when using a capacitor with the shift
 * register latch pin, as different capacitor values affect how quickly the
 * latch can "react" (it's a tradeoff between speed and flicker reduction)
 * 
 * @param int toggle pin
 * @param on true for 'high -> wait -> low', false for 'low -> wait -> high'
 * @param long [description]
 */
void blip(unsigned int pin, bool on, unsigned long us)
{
  digitalWrite(pin, on ? HIGH : LOW);
  if (us > 0) delayMicroseconds(us);
  digitalWrite(pin, on ? LOW : HIGH);
}

