
/*
 * This file is modified by AF6UY (dspmathguru) but parts are copied from
 * http://interface.khm.de/index.php/lab/interfaces-advanced/arduino-dds-sinewave-generator/
 */
/*
 * Original comment in file
 * DDS Sine Generator mit ATMEGS 168
 * Timer2 generates the  31250 KHz Clock Interrupt
 *
 * KHM 2009 /  Martin Nawrath
 * Kunsthochschule fuer Medien Koeln
 * Academy of Media Arts Cologne

 */

#include <stdlib.h>
#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#include <wiring.h>
#endif
#include <avr/pgmspace.h>

typedef unsigned char uchar;
void Setup_timer();

// table of 256 sine values / one sine period / stored in flash memory
const uchar sine256[] PROGMEM = {
    127, 130, 133, 136, 139, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170,
    173, 176, 178, 181, 184, 187, 190, 192, 195, 198, 200, 203, 205, 208, 210,
    212, 215, 217, 219, 221, 223, 225, 227, 229, 231, 233, 234, 236, 238, 239,
    240, 242, 243, 244, 245, 247, 248, 249, 249, 250, 251, 252, 252, 253, 253,
    253, 254, 254, 254, 254, 254, 254, 254, 253, 253, 253, 252, 252, 251, 250,
    249, 249, 248, 247, 245, 244, 243, 242, 240, 239, 238, 236, 234, 233, 231,
    229, 227, 225, 223, 221, 219, 217, 215, 212, 210, 208, 205, 203, 200, 198,
    195, 192, 190, 187, 184, 181, 178, 176, 173, 170, 167, 164, 161, 158, 155,
    152, 149, 146, 143, 139, 136, 133, 130, 127, 124, 121, 118, 115, 111, 108,
    105, 102, 99,  96,  93,  90,  87,  84,  81,  78,  76,  73,  70,  67,  64,
    62,  59,  56,  54,  51,  49,  46,  44,  42,  39,  37,  35,  33,  31,  29,
    27,  25,  23,  21,  20,  18,  16,  15,  14,  12,  11,  10,  9,   7,   6,
    5,   5,   4,   3,   2,   2,   1,   1,   1,   0,   0,   0,   0,   0,   0,
    0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,   10,  11,
    12,  14,  15,  16,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,  37,
    39,  42,  44,  46,  49,  51,  54,  56,  59,  62,  64,  67,  70,  73,  76,
    78,  81,  84,  87,  90,  93,  96,  99,  102, 105, 108, 111, 115, 118, 121,
    124

};

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

int testPin = 2;
int tPin = 1;
byte bb;

double dfreq;
// const double refclk=31372.549;  // =16MHz / 510
const double refclk = 31376.6; // measured

// variables used inside interrupt service declared as voilatile
volatile byte icnt;             // var inside interrupt
volatile byte icnt1;            // var inside interrupt
volatile byte c4ms;             // counter incremented all 4ms
volatile unsigned long phaccu;  // pahse accumulator
volatile unsigned long tword_m; // dds tuning word m

void setup() {
  pinMode(testPin, OUTPUT);
  pinMode(tPin, OUTPUT); // pin11= PWM  output / frequency output

  Setup_timer();

  sbi(TIMSK, OCIE0A); // enable Timer0 Interrupt

  dfreq = 1000.0;                        // initial output frequency = 1000.o Hz
  tword_m = pow(2, 32) * dfreq / refclk; // calulate DDS new tuning word
}

void loop() {}

void Setup_timer() {
  // Prescaler to : 1
  sbi(TCCR0B, CS00);
  cbi(TCCR0B, CS01);
  cbi(TCCR0B, CS02);

  // PWM Mode set to Phase Correct PWM
  cbi(TCCR0A, COM0B0); // clear Compare Match
  sbi(TCCR0A, COM0B1);

  sbi(TCCR0A, WGM00); // Mode 1  / Phase Correct PWM
  cbi(TCCR0A, WGM01);
  cbi(TCCR0B, WGM02);
}

//******************************************************************
// Timer2 Interrupt Service at 31372,550 KHz = 32uSec
// this is the timebase REFCLOCK for the DDS generator
// FOUT = (M (REFCLK)) / (2 exp 32)
// runtime : 8 microseconds ( inclusive push and pop)
ISR(TIMER1_OVF_vect) {

  digitalWrite(testPin, HIGH);

  phaccu = phaccu + tword_m; // soft DDS, phase accu with 32 bits
  icnt = phaccu >> 24;
  OCR0B = pgm_read_byte_near(sine256 + icnt);

  if (icnt1++ == 125) { // increment variable c4ms all 4 milliseconds
    c4ms++;
    icnt1 = 0;
  }

  digitalWrite(testPin, LOW);
}
