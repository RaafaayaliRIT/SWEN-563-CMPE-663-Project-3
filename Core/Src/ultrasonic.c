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

// Add the measurement state and shared data you need
// Decide which values are written by the ISR and read by the foreground

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
	(void)now_us;
	// Add your start conditions and trigger sequence here
	return false;
}

//******************************************************************************************
// Check whether the current measurement needs attention
// A missing echo should produce a timeout result rather than stop the program
// Use elapsed time that still works when the timer wraps
//******************************************************************************************
void Ultrasonic_Service(uint32_t now_us)
{
	(void)now_us;
	// Add your measurement service here
}

//******************************************************************************************
// Give the foreground one consistent measurement result
// The ISR may update shared data while this function is running
// Decide how to protect the copy and mark that result as consumed
//******************************************************************************************
bool Ultrasonic_TakeSample(Ultrasonic_Sample *sample)
{
	(void)sample;
	// Add your result handoff here
	return false; // No measurement has been implemented yet
}

//******************************************************************************************
// Handle the timer events used by your measurement design
// Read captured timestamps, handle the event and clear its flag
// Keep printing, waiting and car decisions out of this ISR
//******************************************************************************************
void TIM2_IRQHandler(void)
{
	// Add your capture and trigger-end handling here
}
