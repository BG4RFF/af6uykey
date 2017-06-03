
/*
 * This file is modified by AF6UY (dspmathguru) using ideas from:
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

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sfr_defs.h>
#include <math.h>
#include <stdlib.h>

#include <usitwislave.h>

typedef int8_t byte;

union getBytes {
  double d;
  uint8_t bs[32];
};

#define TONE_GEN_ADDR (0x4)

#define ERROR (0xFF)
#define AOK (0x00)
#define BOGUS_LENGTH (0x01)
#define READ_TONE_FREQ (0x02)
#define WRITE_TONE_FREQ (0x03)

typedef unsigned char uchar;

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
#define gbi(sfr, bit) (_SFR_BYTE(sfr) & _BV(bit) ? 1 : 0)

#define PWMPIN (DDB1)
#define TESTPIN (DDB3)
#define PLAYBUTTON (DDB4)

volatile bool testVal = false;

double dfreq;
// const double refclk=31372.549;  // =16MHz / 510
const double refclk = 31376.6; // measured

// variables used inside interrupt service declared as voilatile
volatile byte icnt;             // var inside interrupt
volatile byte icnt1;            // var inside interrupt
volatile byte c4ms;             // counter incremented all 4ms
volatile unsigned long phaccu;  // pahse accumulator
volatile unsigned long tword_m; // dds tuning word m

void Setup_timer() {
  cli();

  // Prescaler to : 1
  sbi(TCCR0B, CS00);
  cbi(TCCR0B, CS01);
  cbi(TCCR0B, CS02);

  // PWM Mode set to Phase Correct PWM
  // Clear on up counting, set on down-counting
  cbi(TCCR0A, COM0B0);
  sbi(TCCR0A, COM0B1);

  sbi(TCCR0A, WGM00); // Mode 1  / Phase Correct PWM
  cbi(TCCR0A, WGM01);
  cbi(TCCR0B, WGM02);

  sbi(GTCCR, PSR0);

  sbi(TIMSK, TOIE0); // enable Timer0 Interrupt

  OCR0B = 0x7F;

  sei();
}

void dataCallback(uint8_t input_buffer_length, const uint8_t *input_buffer,
                  uint8_t *output_buffer_length, uint8_t *output_buffer) {
  if (input_buffer_length < 1) {
    *output_buffer_length = 2;
    *output_buffer++ = ERROR;
    *output_buffer++ = BOGUS_LENGTH;
  }

  uint8_t command = input_buffer[0];

  if (command == READ_TONE_FREQ) {
    // read tone frequency
    *output_buffer_length = 1 + sizeof(dfreq);
    output_buffer[0] = READ_TONE_FREQ;
    getBytes bs;
    bs.d = refclk;
    for (unsigned int i = 0; i < sizeof(dfreq); i++) {
      output_buffer[1 + i] = bs.bs[i];
    }
  } else if (command == WRITE_TONE_FREQ) {
    getBytes bs;
    for (unsigned int i = 0; i < sizeof(dfreq); i++) {
      bs.bs[i] = input_buffer[1 + i];
    }

    dfreq = bs.d;
    double hldr = pow(2, 32) * dfreq / refclk; // calulate DDS new tuning word
    cli();
    tword_m = hldr;
    sei();

    *output_buffer_length = 1;
    output_buffer[0] = AOK;
  }
}

void setup() {
  dfreq = 600.0;                         // initial output frequency = 1000.o Hz
  tword_m = pow(2, 32) * dfreq / refclk; // calulate DDS new tuning word

  sbi(DDRB, PWMPIN);     // write
  sbi(DDRB, TESTPIN);    // write
  cbi(DDRB, PLAYBUTTON); // read

  Setup_timer();
}

uint8_t playButton = 0;

void loop() { playButton = gbi(PINB, PINB4); }

ISR(TIM0_OVF_vect) {
  sbi(PORTB, TESTPIN);

  phaccu = phaccu + tword_m; // soft DDS, phase accu with 32 bits
  icnt = phaccu >> 24;
  if (playButton)
    OCR0B = pgm_read_byte_near(sine256 + icnt);
  else
    OCR0B = 0;

  cbi(PORTB, TESTPIN);
}

int main() {
  setup();

  usi_twi_slave(TONE_GEN_ADDR, 0, dataCallback, loop);
}
