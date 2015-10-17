/*
Buttonpad 1x4
(or, 2x2 button layout)

SHIFT REGISTER

  Use a shift register to turn three pins into eight (or more!)
  outputs

  An integrated circuit ("IC"), or "chip", is a self-contained
  circuit built into a small plastic package. (If you look closely
  at your Arduino board you'll see a number of ICs.) There are
  thousands of different types of ICs available that you can use
  to perform many useful functions.

  The 74HC595 shift register in your kit is an IC that has eight
  digital outputs. To use these outputs, we'll use a new interface
  called SPI (Serial Peripheral Interface). It's like the TX and 
  RX you're used to, but has an additional "clock" line that 
  controls the speed of the data transfer. Many parts use SPI
  for communications, so the Arduino offers simple commands called
  shiftIn() and shiftOut() to access these parts.

  This IC lets you use three digital pins on your Arduino to
  control eight digital outputs on the chip. And if you need 
  even more outputs, you can daisy-chain multiple shift registers
  together, allowing an almost unlimited number of outputs from 
  the same three Arduino pins! See the shift register datasheet
  for details:

  http://www.sparkfun.com/datasheets/IC/SN74HC595.pdf

Hardware connections:

  Shift register:

    The shift register has 16 pins. They are numbered counterclockwise starting
    at the pin 1 mark (notch in the end of the chip). See the datasheet for a
    diagram.

      SN74HC595       Buttonpad     ESP-07/12
      ---------       ---------     ---------
    * 1  (QB)         RED +
    * 2  (QC)         GREEN +
    * 3  (QD)         BLUE +
    * 4  (QE)         LED-GND2 -
    * 5  (QF)         LED-GND1 -
    * 6  (QG)         LED-GND3 -
    * 7  (QH)         LED-GND4 -
      8  (GND)                      GND
     ---
      9  (QH*)
      10 (SRCLR*)                   5V
      11 (SRCLK)                    5
      12 (RCLK)                     4
      13 (OE*)                      GND
      14 (SER)                      15
    * 15 (QA)         SWITCH +
      16 (VCC)                      5V

*/

#include <ESP8266WiFi.h>

// Register
// --------
// register bit masks
const byte SWITCH_MASK    = B10000000;
const byte RGB_MASK       = B01110000;
const byte COL_MASK       = B00001111;

// control pins
const int OE_PIN          = 15;
const int SRCLR_PIN       = 2;
const int SER_PIN         = 4;
const int RCLK_PIN        = 0;
const int SRCLK_PIN       = 5;


// Button
// ------
const int BTN_GND_PINS[4]  = {12, 13, 14, 16};  // col: 2, 1, 3, 4


// RGB
// ---
const int NUM_COLS        = 4;
const byte RGB_MAX        = 255;

// Settings
// --------
const int STEP_DELAY      = 32;       // microseconds (default: 24)
const int CYCLE_DELAY     = 32;     // microseconds (default: 0)

// Init
// ----
int column                = -1;
unsigned long was[4]      = {0, 0, 0, 0};
unsigned int rgbs[4][3]   = {{255, 0, 0},{0, 255, 0},{0, 0, 255},{150, 0, 200}};
int btns[4]               = {LOW, LOW, LOW, LOW};

unsigned int rgbsfx[4][3] = {{255, 0, 0},{0, 255, 0},{0, 0, 255},{150, 0, 200}};

// Run
// ---
void setup()
{
  // register pins
  pinMode(OE_PIN, OUTPUT);
  pinMode(SRCLR_PIN, OUTPUT);

  pinMode(SER_PIN, OUTPUT);
  pinMode(RCLK_PIN, OUTPUT);
  pinMode(SRCLK_PIN, OUTPUT);

  for(int i = 0; i < 4; i++) {
    pinMode(BTN_GND_PINS[i], INPUT);
  }

  // set initial pin state
  digitalWrite(OE_PIN, LOW);
  digitalWrite(SRCLR_PIN, HIGH);

  digitalWrite(SER_PIN, LOW);
  digitalWrite(SRCLK_PIN, LOW);
  digitalWrite(RCLK_PIN, LOW);

  // reset register
  bytelatch(0B10001111);
}

void loop()
{
  column = (column + 1) % NUM_COLS;
  sense();
  step(&column);
  if (CYCLE_DELAY) delayMicroseconds(CYCLE_DELAY / NUM_COLS);
  react();
}


// Functions
// ---------

/**
 * @brief Set the state (high, low) of each rgb pin for the current step
 * @details Effectively software pwm emulation (bit-banging). The state of each
 * pin (high/low) is calculated in order to achieve an approximation of the
 * desired workload (0-255) across multiple column cycles.
 * The SN74HC595 doesn't support PWM out (aka "intensity"), but it is plenty
 * fast enough to cycle a few RGB leds.
 */
void step(int *col)
{
  byte data = 1U | ~(1U << (*col + 4U));
  bool on;
  
  // mark start of column cycle
  unsigned long now = micros();

  // turn on if:
  //  workload greater than 0
  //  256µs clock, modulo workload, is less than or equal to half the workload,
  //    plus the between column cycles (to compensate for time the led was off)
  for(unsigned int i = 0; i < 3; i++) {
    on = (rgbsfx[*col][i] > 0) && ((now % 256) % (rgbsfx[*col][i] + 1) <= (((rgbsfx[*col][i] + 1) / 2) + (now - was[*col])));
    if (on) {
      data |= 1U << (i + 1U);
    } else {
      data &= ~(1U << (i + 1U));
    }
  }

  bytelatch(data);
  delayMicroseconds(STEP_DELAY);
  // record end to calculate time between column cycles
  was[*col] = micros();
}

/**
 * @brief Shift byte data into register and toggle data clock (latch)
 * 
 * @param data byte to shift in (least significant bit first)
 */
void bytelatch(byte data)
{
  // make sure clock is low
  digitalWrite(SRCLK_PIN, LOW);
  // shift data into position
  shiftOut(SER_PIN, SRCLK_PIN, MSBFIRST, data);
  // output in parallel
  blip(RCLK_PIN, true, 0);
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

void sense()
{
  btns[column] = digitalRead(BTN_GND_PINS[column]);
}

void react()
{
  int btn = digitalRead(BTN_GND_PINS[column]);
  if (btn && !btns[column]) {
    rgbsfx[column][0] = 255;
    rgbsfx[column][1] = 0;
    rgbsfx[column][2] = 0;
  } else if(btn) {
    rgbsfx[column][0] = 255;
    rgbsfx[column][1] = 255;
    rgbsfx[column][2] = 255;
  } else {
    rgbsfx[column][0] = rgbs[column][0];
    rgbsfx[column][1] = rgbs[column][1];
    rgbsfx[column][2] = rgbs[column][2];
  }
  btns[column] = btn;
}
