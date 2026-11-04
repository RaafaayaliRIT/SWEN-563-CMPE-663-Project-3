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

// Add the measurement state and shared data you need
// Decide which values are written by the ISR and read by the foreground

// State information
static Ultrasonic_State current_state = ULTRASONIC_IDLE_STATE;
static Ultrasonic_Result result = ULTRASONIC_NOT_READY;

// Timestamp variables
static uint32_t trigger_start_us;
static uint32_t measurement_start_us;
static uint32_t t_rise;
static uint32_t t_fall;

// Distance variables
static uint32_t pulse_width;
static uint32_t distance;

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

	// General purpose output mode (01)
	GPIOB->MODER &= ~(GPIO_MODER_MODE5);
	GPIOB->MODER |= GPIO_MODER_MODE5_0;

	// Push-Pull output (0)
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT5);

	// No PU/PD (00)
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD5);

	// Start output pin as 0 (BR5=1)
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
	// Channel 3 as input->TI3 (01)
	TIM2->CCMR2 &= ~(TIM_CCMR2_CC3S);
	TIM2->CCMR2 |= TIM_CCMR2_CC3S_0;

	// Capture every edge, no PS (00)
	TIM2->CCMR2 &= ~(TIM_CCMR2_IC3PSC);

	// No digital filter (0000)
	TIM2->CCMR2 &= ~(TIM_CCMR2_IC3F);

	// Disable capture/compare (0)
	TIM2->CCER &= ~(TIM_CCER_CC3E);

	// Rising edge sensitive
	TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);

	// Enable interrupt for CH3 capture
	TIM2->DIER |= TIM_DIER_CC3IE;

	// Clear stale flag
	TIM2->SR &= ~(TIM_SR_CC3IF); // Also cleared by reading CCR3

	// NVIC for TIM2_IRQ (Unsure about this code. Taken from project 2 implementation)
	NVIC_ClearPendingIRQ(TIM2_IRQn);
	NVIC_SetPriority(TIM2_IRQn, TIM2_INTERRUPT_PRIORITY); // Interrupt priority is taken from starter code in project 2
	NVIC_EnableIRQ(TIM2_IRQn);

	// Enable capture/compare (1)
	TIM2->CCER |= TIM_CCER_CC3E;
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
	GPIOB->BSRR |= (GPIO_BSRR_BS5);

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
	uint64_t trigger_time_elapsed = CalculateTimeElapsed(now_us, trigger_start_us);

	// Determine time since echo pulse start
	uint64_t echo_time_elapsed = CalculateTimeElapsed(now_us, measurement_start_us);

	// End trigger pulse
	if(trigger_time_elapsed > ULTRASONIC_TRIGGER_US && current_state == ULTRASONIC_TRIGGER_STATE)
	{
		// Set TRIG low
		GPIOB->BSRR |= (GPIO_BSRR_BR5);

		// Configure for rising edge
		TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);

		// Save echo start time
		measurement_start_us = now_us;

		// Update current state
		current_state = ULTRASONIC_BURST_STATE;
	}

	// Successful Capture
	if(current_state == ULTRASONIC_COMPLETE_STATE)
	{
		// Update pulse width
		pulse_width = t_fall - t_rise;

		// Calculate distance
		distance = ((double) SPEED_OF_SOUND * (double) (CalculateTimeElapsed(t_fall, t_rise))) / 2.0;

		// Mark result as OK
		result = ULTRASONIC_OK;
	}

	// Timeout
	if(current_state == ULTRASONIC_ECHO_STATE && echo_time_elapsed > ULTRASONIC_TIMEOUT_US)
	{
		// Mark result as timeout
		result = ULTRASONIC_TIMEOUT;

		// Configure for rising edge
		TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);

		// Update current state
		current_state = ULTRASONIC_TIMEOUT_STATE;
	}
}

//******************************************************************************************
// Accessor for the current state.
//******************************************************************************************
Ultrasonic_State GetState()
{
	return current_state; // TODO - ensure this is actually needed and can't be inferred
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
		sample->timestamp_us = timer_get_us();

		// Give "sequence"? TODO - what is sequence?
		sample->sequence;

		// Return true to show the sample was ready
		return true;
	}
	// If a sample is not ready
	else
	{
		// Only give timestamp and result info
		sample->timestamp_us = timer_get_us();
		sample->result = result;

		// Return false to show sample was not ready
		return false;
	}
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
			TIM2->CCER |= TIM_CCER_CC3NP; 						// Set (01)

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
