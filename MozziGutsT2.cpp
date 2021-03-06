/*
 * MozziGuts.cpp
 *
 * Copyright 2012 Tim Barrass.
 *
 * This file is part of Mozzi.
 *
 * Mozzi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mozzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mozzi.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "MozziGutsT2.h"
#include <util/atomic.h>
#include "utils.h"

/** @ingroup core
Sets up the timers for audio and control rate processes. It goes in your
sketch's setup() routine. startMozzi() starts audio interrupts on Timer 2
and control interrupts on Timer 0. The audio rate is currently fixed at 16384
Hz.
@param control_rate_hz Sets how often updateControl() is called. It can be any
power of 2 above and including 64. The practical upper limit for control rate
depends on how busy the processor is, and you might need to do some tests to
find the best setting. It's good to define CONTROL_RATE in your
sketches (eg. "#define CONTROL_RATE 128") because the literal numeric value is
necessary for Oscils to work properly, and it also helps to keep the
calculations in your sketch clear.
 */
void startMozziT2(unsigned int control_rate_hz)
{

	// audio
	pinMode(AUDIO_CHANNEL_1_PIN, OUTPUT);	// set pin to output for audio
	
	 // Set up PWM  with Clock/256 (i.e.  31.25kHz on Arduino 16MHz;
	// and  phase accurate
	TCCR2A = _BV(COM2B1) | _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);
	//   TCCR2A = _BV(COM2A1) | _BV(WGM20);
	TCCR2B = _BV(CS20);
	TIMSK2 = _BV(TOIE2);
	//   sbi (TIMSK2,TOIE2);              // enable Timer2 Interrupt

	AUDIO_CHANNEL_1_OUTPUT_REGISTER = AUDIO_BIAS;
	
// 	Timer1.initialize(1000000UL/AUDIO_RATE);		// set period
// 	Timer1.pwm(AUDIO_CHANNEL_1_PIN, AUDIO_BIAS);		// pwm pin, 50% of Mozzi's duty cycle, ie. 0 signal
// 	Timer1.attachInterrupt(outputAudio);			// call outputAudio() on each interrupt

	// control
	// Using Timer0 for control disables Arduino's time functions
	// but also saves on the interrupts and blocking action of those functions.
	// May add a config option for Using Timer2 instead if needed.
	TimerZero::init(1000000/control_rate_hz,updateControlT2); // set period, attach updateControl()
	TimerZero::start();

	//MozziTimer2::set(1000000/control_rate_hz,updateControl); // set period, attach updateControl()
	//MozziTimer2::start();

	// FrequencyTimer2::setPeriod(2000000UL/control_rate_hz); // would use 1000000, but FrequencyTimer2 sets the period as half
	// FrequencyTimer2::setOnOverflow(updateControl);
	// FrequencyTimer2::enable();

}

ISR(TIMER2_OVF_vect) {
    outputAudioT2();
}

// ring buffer for audio output
#define BUFFER_NUM_CELLS 256
static int output_buffer[BUFFER_NUM_CELLS];

static volatile unsigned char num_out; // shared by audioHook() (in loop()), and outputAudio() (in audio interrupt), where it is changed // test


/** @ingroup core
This is required in Arduino's loop(). If there is room in Mozzi's output buffer,
audioHook() calls updateAudio() once and puts the result into the output
buffer,. If other functions are called in loop() along with audioHook(), see if
they can be moved into updateControl(). Otherwise it may be most efficient to
calculate a block of samples at a time by putting audioHook() in a loop of its
own, rather than calculating only 1 sample for each time your other functions
are called.
@todo try pre-decrement positions and swap gap calc around
*/
void audioHookT2()
{
	static unsigned char num_in = 0;
	unsigned int gap = num_in - num_out; // wraps to a big number if it's negative

	if(gap < BUFFER_NUM_CELLS) // prevent writing over cells which haven't been output yet
	{
		output_buffer[num_in++] = updateAudioT2() + AUDIO_BIAS;
	}
}


/* This is the callback routine attached to the Timer1 audio interrupt.
It moves sound data from the output buffer to the Arduino output register,
running at AUDIO_RATE.
*/
inline
static void outputAudioT2()
{ // takes 1 us or shorter, digital scope seems to have trouble catching it
	//SET_PIN13_HIGH;
	AUDIO_CHANNEL_1_OUTPUT_REGISTER =  output_buffer[num_out++];
	//SET_PIN13_LOW;
}

// Unmodified TimerOne.cpp has TIMER3_OVF_vect.
// Watch out if you update the library file.
// The symptom will be no sound.
// ISR(TIMER1_OVF_vect)
// {
// 	Timer1.isrCallback();
// }
