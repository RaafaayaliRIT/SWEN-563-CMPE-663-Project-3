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

#define SPEED_OF_SOUND_MM_US (0.343)

// Add the measurement state and shared data you need
// Decide which values are written by the ISR and read by the foreground
Ultrasonic_State currentState = ULTRASONIC_IDLE_STATE;
uint32_t trigger_start_us;
uint32_t measurement_start_us;
uint32_t t_rise;
uint32_t t_fall;
uint32_t pulse_width;
Ultrasonic_Result result = ULTRASONIC_NOT_READY;
uint32_t distance;


Ultrasonic_State getState()
{
	return currentState;
}


bool Ultrasonic_HasNewSample()
{
	return (result == ULTRASONIC_OK && currentState == ULTRASONIC_COMPLETE_STATE);
}


uint64_t CalculateTimeElapsed(uint32_t now_us, uint32_t previous_us)
{
	// Create variable to hold final time elapsed
	uint64_t time_elapsed;

	// If there was not counter reset
	if(now_us >= previous_us)
	{
		time_elapsed = now_us - previous_us;
	}
	// If there was a counter reset
	else
	{
		time_elapsed = (now_us + TIM_CNT_CNT) - previous_us;
	}

	// Return final time elapsed
	return time_elapsed;
}

uint32_t Ultrasonic_GetDistance()
{
	return distance;
}


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
	// Raise TRIG and record time
	// set state trig high
	// Return true

	// If not idle, reject and return false
	if(currentState != ULTRASONIC_IDLE_STATE)
	{
		return false;
	}

	// Set the trigger output pin high
	GPIOB->BSRR |= (GPIO_BSRR_BS5);

	// Record trigger pulse start time
	trigger_start_us = now_us;

	// Update current state
	currentState = ULTRASONIC_TRIGGER_STATE;

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
	// If state is TRIGGER_HIGH and at least 10 us elapsed
		// Set TRIG LOW
		// Arm/confirm rising-edge capture
		// Save measurement_start_us
		// Set state = WAIT_ECHO
	// If state is WAIT_ECHO and Echo completed
		// Convert pulse width to distance
		// Publish the new sample
		// Set state = COMPLETE
	// If state is WAIT_ECHO and timeout elapsed
		// Mark timeout
		// Restore capture for the next request
		// Set state = TIMEOUT

	// Determine time since trigger pulse start
	uint64_t trigger_time_elapsed = CalculateTimeElapsed(now_us, trigger_start_us);

	// Determine time since echo pulse start
	uint64_t echo_time_elapsed = CalculateTimeElapsed(now_us, measurement_start_us);

	// End trigger pulse
	if(trigger_time_elapsed > ULTRASONIC_TRIGGER_US && currentState == ULTRASONIC_TRIGGER_STATE)
	{
		// Set TRIG low
		GPIOB->BSRR |= (GPIO_BSRR_BR5);

		// Configure for rising edge
		TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);

		// Save echo start time
		measurement_start_us = now_us;

		// Update current state
		currentState = ULTRASONIC_BURST_STATE;
	}

	// Successful Capture
	if(currentState == ULTRASONIC_COMPLETE_STATE)
	{
		// Update pulse width
		pulse_width = t_fall - t_rise;

		// Calculate distance
		distance = ((double) SPEED_OF_SOUND_MM_US * (double) (CalculateTimeElapsed(t_fall, t_rise))) / 2.0;

		// Mark result as OK
		result = ULTRASONIC_OK;
	}

	// Timeout
	if(currentState == ULTRASONIC_ECHO_STATE && echo_time_elapsed > ULTRASONIC_TIMEOUT_US)
	{
		// Mark result as timeout
		result = ULTRASONIC_TIMEOUT;

		// Configure for rising edge
		TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);

		// Update current state
		currentState = ULTRASONIC_TIMEOUT_STATE;
	}
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

	// Note that the interrupt does not edit the sample result, the sample result is
	// updated in Ultrasonic_Service.

	if(result != ULTRASONIC_NOT_READY)
	{
		sample->echo_us = pulse_width; 				// Pulse width
		sample->distance_mm = distance; 			// Calculated distance
		sample->sequence; 							// TODO - ?
		sample->timestamp_us = timer_get_us(); 		// Current time (TODO - confirm this is the timestamp requested)
		sample->result = result;					// Result

		return true;
	}
	else
	{
		sample->timestamp_us = timer_get_us();
		sample->result = result;
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
		// Read CCR3
		// Clear the channel 3 capture flag as required
		// If waiting for rising edge
			// Save t_rise
			// Configure the next capture for falling edge
			// Set capture state to WAIT_FALL
		// Else if waiting for falling edge
			// Save t_fall
			// Mark the sample complete
			// Restore rising-edge capture for the next measurement

	// If CC3IF is set
	if((TIM2->SR & TIM_SR_CC3IF) == TIM_SR_CC3IF)
	{
		// If echo has not been started
		if(currentState == ULTRASONIC_BURST_STATE)
		{
			// Record starting echo time
			t_rise = TIM2->CCR3;

			// Configure for falling edge
			TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP); 	// Clear (00)
			TIM2->CCER |= TIM_CCER_CC3NP; 						// Set (01)

			// Update current state
			currentState = ULTRASONIC_ECHO_STATE;
		}
		// If waiting for echo completion
		else if(currentState == ULTRASONIC_ECHO_STATE)
		{
			// Record ending echo time
			t_fall = TIM2->CCR3;

			// Configure for rising edge
			TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP);

			// Update current state
			currentState = ULTRASONIC_COMPLETE_STATE;
		}
		// Clear the channel 3 capture flag if in neither state
		else
		{
			// Clear interrupt flag
			TIM2->SR &= ~(TIM_SR_CC3IF);
		}
	}
}
