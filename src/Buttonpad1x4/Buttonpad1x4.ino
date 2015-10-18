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

    SN74HC595      SPI    Buttonpad     ESP-07/12
    ---------      ---    ---------     ---------
    * 1  (QB)             RED +
    * 2  (QC)             GREEN +
    * 3  (QD)             BLUE +
    * 4  (QE)             LED-GND2 -
    * 5  (QF)             LED-GND1 -
    * 6  (QG)             LED-GND3 -
    * 7  (QH)             LED-GND4 -
      8  (GND)                          GND
     ---    
      9  (QH*)    
      10 (SRCLR*)                       5V
      11 (SRCLK)   SCLK                 14 (SCL)
      12 (RCLK)    CS                   15
      13 (OE*)                          GND
      14 (SER)     MOSI                 13
    * 15 (QA)             SWITCH +
      16 (VCC)                          5V
         (NC)      MISO                 12 (NC)
*/

#include <ESP8266WiFi.h>

// Gamma correction
const uint8_t PROGMEM GAMMA[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };


// Register
// --------
// register bit masks
static const byte SWITCH_MASK    = B10000000;
static const byte RGB_MASK       = B01110000;
static const byte COL_MASK       = B00001111;

// control pins
static const int OE_PIN          = 15;
static const int SRCLR_PIN       = 2;
static const int SER_PIN         = 4;
static const int RCLK_PIN        = 0;
static const int SRCLK_PIN       = 5;


// Button
// ------
static const int BTN_GND_PINS[4]  = {12, 13, 14, 16};  // col order: 2, 1, 3, 4


// RGB
// ---
static const int NUM_COLS        = 4;
static const byte RGB_MAX        = 255;

// Settings
// --------
static const int STEP_DELAY      = 2;        // microseconds (default: 24)
static const int CYCLE_DELAY     = 2;        // microseconds (default: 0)

static const int BTN_DEBOUNCE    = 150;      // microseconds (default: 100)

// Init
// ----
static int column                = -1;
static unsigned long prev[4]     = {0, 0, 0, 0};
static unsigned int rgbs[4][3]   = {{200, 0, 0},{0, 200, 0},{0, 0, 200},{40, 40, 40}};
static unsigned int rgbsfx[4][3] = {{200, 0, 0},{0, 200, 0},{0, 0, 200},{40, 40, 40}};
static unsigned long btns[4]     = {0, 0, 0, 0};

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
  // bytelatch(0B10001111);
}

void loop()
{
  column = (column + 1) % NUM_COLS;
  sense();
  react();
  step();
  if (CYCLE_DELAY) delayMicroseconds(CYCLE_DELAY / NUM_COLS);
}


// Functions
// ---------

/**
 * @brief read inputs (buttons)
 */
void sense()
{
  if(!digitalRead(BTN_GND_PINS[column])) btns[column] = false;
  else if (!btns[column]) btns[column] = micros();
}

/**
 * @brief modify baseline rgb values based on input stimuli
 * 
 */
void react()
{
  if(btns[column] && (micros() > (btns[column] + BTN_DEBOUNCE))) {
    rgbsfx[column][0] = rgbsfx[column][1] = rgbsfx[column][2] = 255;
  } else {
    rgbsfx[column][0] = pgm_read_byte(&GAMMA[rgbs[column][0]]);
    rgbsfx[column][1] = pgm_read_byte(&GAMMA[rgbs[column][1]]);
    rgbsfx[column][2] = pgm_read_byte(&GAMMA[rgbs[column][2]]);
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
void step()
{
  byte data = 1U | ~(1U << (column + 4U));
  bool on;
  
  // mark start of column cycle
  unsigned long now = micros();

  // turn on if:
  //  workload is 255, or greater than 0 and:
  //  the 255µs clock, modulo workload, is less than or equal to half the workload
  //    plus the time between the prev and current cycle (to compensate for led being off)
  for(unsigned int i = 0; i < 3; i++) {
    on = (rgbsfx[column][i] == 255) || (rgbsfx[column][i] && (((now - (now - prev[column])) % 255) * ((double)rgbsfx[column][i] / 255.0)) > ((double)rgbsfx[column][i] / 2.0));
    if (on) {
      data |= 1U << (i + 1U);
    } else {
      data &= ~(1U << (i + 1U));
    }
  }

  // set output data
  bytelatch(&data);
  if(STEP_DELAY) delayMicroseconds(STEP_DELAY);
  // record end to calculate time between column cycles
  prev[column] = micros();
}

/**
 * @brief Shift byte data into register and toggle latch
 * 
 * @param data byte to shift in (least significant bit first)
 */
void bytelatch(byte *data)
{
  espShiftOut(*data);
  blip(RCLK_PIN, HIGH, 0);
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
  digitalWrite(pin, on);
  if (us > 0) delayMicroseconds(us);
  digitalWrite(pin, !on);
}

void espShiftOut(byte data)
{
  byte i = 8;
  do{
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << SRCLK_PIN);
    
    if(data & 0x80) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << SER_PIN);
    else GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << SER_PIN);
    
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << SRCLK_PIN);
    
    data <<= 1;
  } while(--i);
  return;
}
