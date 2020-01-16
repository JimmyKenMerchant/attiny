/**
 * main.c
 *
 * Author: Kenta Ishii
 * License: 3-Clause BSD License
 * License URL: https://opensource.org/licenses/BSD-3-Clause
 *
 */

#define F_CPU 4800000UL // Default 4.8Mhz to ATtiny13
#include <stdlib.h>
#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/delay_basic.h>

#define CALIB_OSCCAL 0x03 // Frequency Calibration for Individual Difference at VCC = 3.3V
#define VOLTAGE_BIAS 0x80 // Decimal 128 on Noise Off

/**
 * Output from PB0 (OC0A)
 * Output from PB1 (OC0B)
 * Input from PB2 (Bit[0]), Set by Detecting Low
 * Input from PB3 (Bit[1]), Set by Detecting Low
 * Input from PB4 (Bit[2]), Set by Detecting Low
 * Bit[2:0]:
 *     0b000: Stop Sequencer
 *     0b001: Play Sequence No.1
 *     0b010: Play Sequence No.2
 *     0b011: PLay Sequence No.3
 *     0b100: PLay Sequence No.4
 *     ...
 */

#define SAMPLE_RATE (double)(F_CPU / 256) // 18750 Samples per Seconds
#define SEQUENCER_INTERVAL 1
#define SEQUENCER_COUNTUPTO 576 // (576 Bytes * 8 Bits) / 18750 Samples = Approx. 0.24576 Seconds
#define SEQUENCER_COUNTUPTO_BIT 4608
#define SEQUENCER_SEQUENCENUMBER 1 // Maximum Number of Sequence
#define DPCM_DELTA 2 // Delta of Differential Pulse Code Modulation

/* Global Variables without Initialization to Define at .bss Section and Squash .data Section */

uint8_t sequencer_count_start;
uint16_t sequencer_interval_count;
uint16_t sequencer_count_update;

uint8_t sequencer_volume;

/**
 * DPCM Sequences for OC0A and OC0B
 * Bit[7:0]: 1 as Plus Volume (+2), 0 as Minus Volume (-2), Read from LSB
 */
uint8_t const sequencer_array_a[SEQUENCER_SEQUENCENUMBER][SEQUENCER_COUNTUPTO] PROGMEM = { // Array in Program Space
	{0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111,
	 0b11111111,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b00000000,0b00000000,
	 0b00000000,0b00000000,0b00000000,0b00000000,0b11111111,0b11111111,0b11111111,0b11111111}, // Sequence No.1, Triangle 146.484375 Hz
};

int main(void) {

	/* Declare and Define Local Constants and Variables */
	uint8_t const pin_button1 = _BV(PINB2); // Assign PB2 as Button Input
	uint8_t const pin_button2 = _BV(PINB3); // Assign PB3 as Button Input
	uint8_t const pin_button3 = _BV(PINB4); // Assign PB4 as Button Input
	uint16_t sequencer_count_last = 0;
	uint8_t input_pin;
	uint8_t sequencer_byte;
	uint8_t osccal_default; // Calibrated Default Value of OSCCAL

	/* Clock Prescaler for 4.8Mhz */
	CLKPR = _BV(CLKPCE); // Clock Prescaler Change Enable
	CLKPR = _BV(CLKPS0); // Clock Division Factor 2

	/* Initialize Global Variables */
	sequencer_count_start = 0;
	sequencer_interval_count = 0;
	sequencer_count_update = 0;
	sequencer_volume = VOLTAGE_BIAS;

	/* Clock Calibration */
	osccal_default = OSCCAL + CALIB_OSCCAL; // Frequency Calibration for Individual Difference at VCC = 3.3V
	OSCCAL = osccal_default;

	/* I/O Settings */
	DIDR0 = _BV(PB5)|_BV(PB1)|_BV(PB0); // Digital Input Disable
	PORTB = _BV(PB4)|_BV(PB3)|_BV(PB2); // Pullup Button Input (There is No Internal Pulldown)
	DDRB = _BV(DDB1)|_BV(DDB0); // Bit Value Set PB0 (OC0A) and PB1 (OC0B)

	/* Counter/Timer */
	// Counter Reset
	TCNT0 = 0;
	// Clear Compare A
	OCR0A = VOLTAGE_BIAS;
	// Clear Compare B
	OCR0B = VOLTAGE_BIAS;
	// Set Timer/Counter0 Overflow Interrupt for "ISR(TIM0_OVF_vect)"
	TIMSK0 = _BV(TOIE0);
	// Select Fast PWM Mode (3) and Output from OC0A Non-inverted and OC0B Non-inverted
	TCCR0A = _BV(WGM01)|_BV(WGM00)|_BV(COM0B1)|_BV(COM0A1);
	// Start Counter with I/O-Clock 4.8MHz / ( 1 * 256 ) = 18750Hz
	TCCR0B = _BV(CS00);

	while(1) {
		input_pin = 0;
		if ( ! (PINB & pin_button1) ) {
			input_pin |= 0b01;
		}
		if ( ! (PINB & pin_button2) ) {
			input_pin |= 0b10;
		}
		if ( ! (PINB & pin_button3) ) {
			input_pin |= 0b100;
		}
		if ( input_pin ) {
			if ( ! sequencer_count_start || sequencer_count_update != sequencer_count_last ) {
				if ( sequencer_count_update >= SEQUENCER_COUNTUPTO_BIT ) sequencer_count_update = SEQUENCER_COUNTUPTO_BIT;
				sequencer_count_last = sequencer_count_update;
				if ( ! sequencer_count_start ) {
					TCNT0 = 0; // Counter Reset
					TIFR0 |= _BV(TOV0); // Clear Set Timer/Counter0 Overflow Flag by Logic One
					sequencer_count_start = 1;
					sei(); // Start to Issue Interrupt
				}
				if ( input_pin >= SEQUENCER_SEQUENCENUMBER ) input_pin = SEQUENCER_SEQUENCENUMBER;
				if ( sequencer_count_last < SEQUENCER_COUNTUPTO_BIT ) {
					sequencer_byte = pgm_read_byte(&(sequencer_array_a[input_pin - 1][sequencer_count_last >> 3]));
					if ( sequencer_byte & (0x1 << (sequencer_count_last & 0b111)) ) {
						sequencer_volume += DPCM_DELTA;
					} else {
						sequencer_volume -= DPCM_DELTA;
					}
					OCR0A = sequencer_volume;
					OCR0B = sequencer_volume;
				}
			}
		} else {
			if ( SREG & _BV(SREG_I) ) { // If Global Interrupt Enable Flag Is Set
				cli(); // Stop to Issue Interrupt
				sequencer_count_start = 0;
				sequencer_interval_count = 0;
				sequencer_count_update = 0;
				sequencer_count_last = 0;
				sequencer_volume = VOLTAGE_BIAS;
				OCR0A = sequencer_volume;
				OCR0B = sequencer_volume;
			}
		}
	}
	return 0;
}

ISR(TIM0_OVF_vect) {
	if ( sequencer_count_start ) { // If Not Zero, Sequencer Is Outstanding
		sequencer_interval_count++;
		if ( sequencer_interval_count >= SEQUENCER_INTERVAL ) {
			sequencer_interval_count = 0;
			sequencer_count_update++;
		}
	}
}
