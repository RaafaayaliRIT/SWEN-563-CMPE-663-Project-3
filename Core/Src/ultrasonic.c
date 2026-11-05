/*
 * ultrasonic.c
 * Phase 2 - HC-SR04 on D4 / PB5 and D6 / PB10
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */

#include "ultrasonic.h"
#include "timer.h"
#include "stm32l476xx.h"
#include "custom_functions.h"
#include "LED.h"

// Add the measurement state and shared data you need
// Decide which values are written by the ISR and read by the foreground


// Distance conversion: 343 m/s = 0.343 mm/us, and the echo is a round trip,
// so distance_mm = echo_us * 0.343 / 2 = echo_us * 343 / 2000.
#define SOUND_MM_PER_US_NUM 343
#define SOUND_MM_PER_US_DEN 2000


// State information
static volatile Ultrasonic_State current_state = ULTRASONIC_IDLE_STATE; // since this gets changed in the IRQ we need to set it to volatile
static Ultrasonic_Result result = ULTRASONIC_NOT_READY;

// Timestamp variables
static uint32_t trigger_start_us;
static uint32_t measurement_start_us;

// if the IRQ alters a variable it needs to be volatile
static volatile uint32_t t_rise;
static volatile uint32_t t_fall;

static uint32_t trigger_time_elapsed;
static uint32_t echo_time_elapsed;

// Distance variables
static uint32_t pulse_width;
static uint32_t distance;


static uint32_t sequence_count = 0;
static uint32_t sample_time_us;

//******************************************************************************************
// Set up PB5 for Trigger and PB10 for Echo capture
// TIM2 is already running with 1 us counts
// Add the capture and trigger timing you need without resetting the timebase
// Finish the interrupt handler before enabling its interrupt sources
//******************************************************************************************
void Ultrasonic_Init(void)
{
	// --- TRIG GPIO PATH ---
	// Enable GPIOB register access (1)
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;


	(void) RCC->AHB2ENR; // dummy read-back: make sure the GPIOB clock is on before configuring it


	// General purpose output mode (01)
	GPIOB->MODER &= ~(GPIO_MODER_MODE5);
	GPIOB->MODER |= GPIO_MODER_MODE5_0;

	// Push-Pull output (0)
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT5);

	// No PU/PD (00)
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD5);

	// Start output pin as 0 (BR5=1)
	// I don't think we can read this value, (it's always 0) but we can set it
	// GPIOB->BSRR = (GPIO_BSRR_BR5);
	GPIOB->BSRR |= (GPIO_BSRR_BR5); // Note that you set the BR5 bit to reset OD5.

	// --- ECHO GPIO PATH ---
	// Alternate function mode (10)
	GPIOB->MODER &= ~(GPIO_MODER_MODE10);
	GPIOB->MODER |= GPIO_MODER_MODE10_1;

	// Alternate function 1, TIM2_CH3 (0001)
	GPIOB->AFR[1] &= ~(GPIO_AFRH_AFSEL10);
	GPIOB->AFR[1] |= (GPIO_AFRH_AFSEL10_0);

	// No PU/PD (00)
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD10);

	// OTYPER/OSPEEDR unnecessary since it is not driving the output.

	// --- TIM2_CH3 Input Capture ---
	// Disable capture/compare (0)
	TIM2->CCER &= ~(TIM_CCER_CC3E);

	// Channel 3 as input->TI3 (01)
	TIM2->CCMR2 &= ~(TIM_CCMR2_CC3S);
	TIM2->CCMR2 |= TIM_CCMR2_CC3S_0;

	// Capture every edge, no PS (00)
	TIM2->CCMR2 &= ~(TIM_CCMR2_IC3PSC);

	// No digital filter (0000)
	TIM2->CCMR2 &= ~(TIM_CCMR2_IC3F);


	// Rising edge sensitive (CC3P = 0, CC3NP = 0)
	TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);


	// Clear stale flag
	TIM2->SR = ~(TIM_SR_CC3IF | TIM_SR_CC3OF)	;
	//TIM2->SR &= ~(TIM_SR_CC3IF); // Also cleared by reading CCR3

	// Enable interrupt for CH3 capture
	TIM2->DIER |= TIM_DIER_CC3IE;



	// NVIC for TIM2_IRQ (Unsure about this code. Taken from project 2 implementation)
	NVIC_ClearPendingIRQ(TIM2_IRQn);
	NVIC_SetPriority(TIM2_IRQn, TIM2_INTERRUPT_PRIORITY); // Interrupt priority is taken from starter code in project 2
	NVIC_EnableIRQ(TIM2_IRQn);

	// Enable capture/compare (1)
	TIM2->CCER |= TIM_CCER_CC3E;

	// Initialize software state to IDLE (guide's last init step)
	current_state = ULTRASONIC_IDLE_STATE;
	result = ULTRASONIC_NOT_READY;
}

//******************************************************************************************
// Start one measurement when the sensor is available
// Keep Trigger high for at least 10 us and leave time between attempts
// Arrange for Trigger to end while the rest of the application keeps running
//******************************************************************************************
bool Ultrasonic_Start(uint32_t now_us)
{
	// If not idle, reject and return false
	if(current_state != ULTRASONIC_IDLE_STATE)
	{
		return false;
	}

	// Set the trigger output pin high
	//GPIOB->BSRR |= (GPIO_BSRR_BS5);
	GPIOB->BSRR = (GPIO_BSRR_BS5); // I'm confused on where you found this information?

	// Record trigger pulse start time
	trigger_start_us = now_us;

	// Update current state
	current_state = ULTRASONIC_TRIGGER_STATE;

	// Return true
	return true;
}

//******************************************************************************************
// Check whether the current measurement needs attention
// A missing echo should produce a timeout result rather than stop the program
// Use elapsed time that still works when the timer wraps
//******************************************************************************************
void Ultrasonic_Service(uint32_t now_us)
{
	// Determine time since trigger pulse start
	trigger_time_elapsed = timer_elapsed_us(trigger_start_us, now_us);


	// If state is TRIGGER_HIGH and at least 10 us elapsed
	if(current_state == ULTRASONIC_TRIGGER_STATE && trigger_time_elapsed > ULTRASONIC_TRIGGER_US){

		//confirm rising-edge capture
		TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);

		// Save measurement_start_us (the timeout is measured from here)
		measurement_start_us = now_us;

		// set the state
		current_state = ULTRASONIC_BURST_STATE;

		//set trig to low, (we want to set this after we set the internal state of the sensor)
		GPIOB->BSRR = GPIO_BSRR_BR5;
	}


	// If state is WAIT_ECHO and Echo completed
	if(current_state == ULTRASONIC_COMPLETE_STATE && result != ULTRASONIC_OK){
		// Publish the new sample
		pulse_width = t_fall - t_rise;
		distance = (pulse_width * SOUND_MM_PER_US_NUM) / SOUND_MM_PER_US_DEN;
		sample_time_us = t_fall;    // when the echo actually ended
		sequence_count++; // one number per published sample
		result = ULTRASONIC_OK;
	}

	// Must come after the trigger block: that block resets measurement_start_us for a new measurement, and computing this earlier would use the old start time.
	echo_time_elapsed = timer_elapsed_us(measurement_start_us, now_us);

	//If state is WAIT_ECHO and timeout elapsed
	// we are checking both the burst state and the echo state since both of these states can enter a timeout
	// BURST: waiting for the rising edge
	// ECHO: waiting for falling edge (30ms timeout)
	if((current_state == ULTRASONIC_BURST_STATE || current_state == ULTRASONIC_ECHO_STATE) && echo_time_elapsed > ULTRASONIC_TIMEOUT_US){

		// mark timeout
		result = ULTRASONIC_TIMEOUT;

		// Restore capture for the next request
		TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);

		// Set state = TIMEOUT (TakeSample recovers it to IDLE)
		current_state = ULTRASONIC_TIMEOUT_STATE;
	}
}

//******************************************************************************************
// Accessor for the current state.
//******************************************************************************************
Ultrasonic_State GetState()
{
	return current_state;
}

//******************************************************************************************
// Determine if the result is valid and the current state is "COMPLETE."
//******************************************************************************************
bool Ultrasonic_HasNewSample()
{
	return (result == ULTRASONIC_OK && current_state == ULTRASONIC_COMPLETE_STATE);
}

//******************************************************************************************
// Give the foreground one consistent measurement result
// The ISR may update shared data while this function is running
// Decide how to protect the copy and mark that result as consumed
//******************************************************************************************
bool Ultrasonic_TakeSample(Ultrasonic_Sample *sample)
{
	// ISR only updates current state, t_rise, t_fall, and various registers. None of this
	// information is used in the struct, so no additional protection is needed.

	// Note that the interrupt does not directly edit the ultrasonic result, the ultrasonic result is
	// updated in Ultrasonic_Service.

	// Ensure given pointer is not null
	if(sample == NULL)
	{
		return false;
	}

	// If a sample is ready
	if(Ultrasonic_HasNewSample())
	{
		// Give distance information
		sample->echo_us = pulse_width;
		sample->distance_mm = distance;
		sample->result = result;

		// Give timestamp
		sample->timestamp_us = sample_time_us;

		// Give sequence
		sample->sequence = sequence_count;


		//mark the sample as consumed
		result = ULTRASONIC_NOT_READY;
		current_state = ULTRASONIC_IDLE_STATE;

		// Return true to show the sample was ready
		return true;
	}

	// timeout needs to go back to idle state
	if(current_state == ULTRASONIC_TIMEOUT_STATE){

		sample->result = ULTRASONIC_TIMEOUT;
		result = ULTRASONIC_NOT_READY;
		current_state = ULTRASONIC_IDLE_STATE;
		return false;
	}

	return false;
}

void Ultrasonic_Reset(uint32_t now_us)
{
	//// Return the hardware to its idle setup, in case Reset is called mid-measurement
	GPIOB->BSRR = GPIO_BSRR_BR5; // TRIG low (could be stuck at high if reset during TRIGGER)
	TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP); // rising edge, could be left at failing edge during ECHO

	current_state = ULTRASONIC_IDLE_STATE;
	result = ULTRASONIC_NOT_READY;

	// Timestamp variables
	trigger_start_us = now_us;
	measurement_start_us = now_us;
	t_rise = 0;
	t_fall = 0;

	// Distance variables
	pulse_width = 0;
	distance = 0;
}

//******************************************************************************************
// Handle the timer events used by your measurement design
// Read captured timestamps, handle the event and clear its flag
// Keep printing, waiting and car decisions out of this ISR
//******************************************************************************************
void TIM2_IRQHandler(void)
{
	// If CC3IF is set
	if((TIM2->SR & TIM_SR_CC3IF) == TIM_SR_CC3IF)
	{
		// If echo has not been started
		if(current_state == ULTRASONIC_BURST_STATE)
		{
			// Record starting echo time
			t_rise = TIM2->CCR3;

			// Configure for falling edge
			TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP); 	// Clear (00)
			TIM2->CCER |= TIM_CCER_CC3P; 						// Set (01)

			// Update current state
			current_state = ULTRASONIC_ECHO_STATE;
		}
		// If waiting for echo completion
		else if(current_state == ULTRASONIC_ECHO_STATE)
		{
			// Record ending echo time
			t_fall = TIM2->CCR3;

			// Configure for rising edge
			TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);

			// Update current state
			current_state = ULTRASONIC_COMPLETE_STATE;
		}
		// Clear the channel 3 capture flag if in neither state
		else
		{
			// Clear interrupt flag
			TIM2->SR &= ~(TIM_SR_CC3IF);
		}
	}
}
