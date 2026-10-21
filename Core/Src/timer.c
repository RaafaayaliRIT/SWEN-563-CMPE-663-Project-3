/*
 * timer.c
 *
 *  Created on: Feb 2, 2026
 *      Author: Mitesh Parikh
 *
 */
 
#include "timer.h"
#include "stm32l476xx.h"

// Timer Configuration 
// 	- System clock = 80 MHz
//	- TIM2 input clock = 80 MHz
//	To get 1 MHz timer:
//		PSC = (80,000,000 / 1,000,000) - 1 = 79
#define TIM2_PRESCALER_1MHZ 79U

// ----------------------------
// TIM2 Microsecond Timer Initialization
// ----------------------------
void timer_init(void)
{
    // Enable TIM2 clock
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    (void)RCC->APB1ENR1;   // Ensure write completes

	// Clear status flags to avoid leftover state from previous runs
	TIM2->SR = 0U;   

    // Disable timer during configuration
    TIM2->CR1 &= ~TIM_CR1_CEN;

    // Up-counting, no special modes
    TIM2->CR1 = 0U;

    // Prescaler: 80 MHz → 1 MHz
    TIM2->PSC = TIM2_PRESCALER_1MHZ;

    // Free-running 32-bit counter
    TIM2->ARR = 0xFFFFFFFFU;

    // Reset counter
    TIM2->CNT = 0U;

    // Load prescaler immediately
    TIM2->EGR = TIM_EGR_UG;

    // Enable timer
    TIM2->CR1 |= TIM_CR1_CEN;
}

// ----------------------------
// Helper function to START Timer
// ----------------------------
void timer_start(void)
{
    TIM2->CR1 |= TIM_CR1_CEN;
}

// ----------------------------
// Helper function to STOP Timer
// ----------------------------
void timer_stop(void)
{
    TIM2->CR1 &= ~TIM_CR1_CEN;
}

// ----------------------------
// Helper function to RESET Timer
// ----------------------------
void timer_reset(void)
{
    TIM2->CNT = 0U;
}

// ----------------------------
// Get current timestamp in microseconds
// ----------------------------
uint32_t timer_get_us(void)
{
    return TIM2->CNT;
}

// ----------------------------
// Compute elapsed time in microseconds
// ----------------------------
uint32_t timer_elapsed_us(uint32_t start_us, uint32_t now_us)
{
    return (uint32_t)(now_us - start_us); // unsigned arithmetic handles wrap
}
