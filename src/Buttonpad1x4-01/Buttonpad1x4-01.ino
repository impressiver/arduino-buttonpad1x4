/*
*
*/

#include <ESP8266WiFi.h>

// Hardware
// --------
// rgb
const int rgbPins[3]  = {13, 12, 14};

// button gnd
const int btnPin      = 16;

// shift register pins
const int dataPin     = 4;
const int clockPin    = 5;
const int latchPin    = 15;


// Init
// ----
unsigned int rgbs[4][3]   = {{220, 255, 0},{0, 255, 0},{0, 0, 255},{255, 0, 100}};
unsigned long was[4]      = {0, 0, 0, 0};
bool btns[4]              = {false, false, false, false};
int col                   = -1;


// Run
// ---
void setup()
{
  // register pins
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  // button pin
  pinMode(btnPin, INPUT);

  digitalWrite(dataPin, LOW);
  digitalWrite(clockPin, LOW);
  digitalWrite(latchPin, LOW);

  // set rgb off
  for(int i = 0; i < 3; i++) {
    pinMode(rgbPins[i], OUTPUT);
    // disable pwm
    analogWrite(rgbPins[i], 0);
  }

  // reset registers
  bytelatch(0x1);
}

void loop()
{
  step();
}


// Functions
// ---------

void step() {
  col = (col + 1) % 4;

  stepOut();
  rgbOut();
  rgbOff();
}

void stepOut()
{
  bytelatch(~(0x1 << col));
}

/**
 * @brief Set the state (high, low) of each rgb pin for the current step
 * @details Effectively software pwm emulation (bit-banging). The state of each
 * pin (high/low) is calculated in order to achieve an approximation of the
 * desired workload (0-255) across multiple column cycles.
 * (I found it difficult to reduce flicker using a shift register and the
 * built-in Arduino pwm functions due to out-of-sync issues. It *is* possible to
 * configure timer registers and aleviate flicker, but the settings are
 * hardware-dependent and I wanted something "good enough" to use on multiple
 * boards without modification)
 */
void rgbOut()
{
  unsigned int i;
  bool on = false;
  
  // mark start of column cycle
  unsigned long now = micros();

  // for each rgb pin, turn on if:
  //  workload greater than 0
  //  256µs clock, modulo workload, is less than or equal to half the workload,
  //    plus the between column cycles (to compensate for time the led was off)
  for(i = 0; i < 3; i++) {
    on = (rgbs[col][i] > 0) && ((now % 256) % (rgbs[col][i] + 1) <= (((rgbs[col][i] + 1) / 2) + (now - was[col])));
    digitalWrite(rgbPins[i], on ? HIGH : LOW);
  }
  // let it shine (for a few µs)
  delayMicroseconds(10);
  // record end to calculate time between column cycles
  was[col] = micros();
}

/**
 * @brief turn all rgb pins low
 */
void rgbOff()
{
  for(unsigned int i = 0; i < 3; i++) {
    digitalWrite(rgbPins[i], LOW);
  }
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
  shiftOut(dataPin, clockPin, LSBFIRST, data);
  // output in parallel
  blip(latchPin, true, 5);
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
  delayMicroseconds(us);
  digitalWrite(pin, on ? LOW : HIGH);
}
