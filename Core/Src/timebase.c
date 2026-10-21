/*
 * timebase.c
 * Supplied time source for Project 3 - Systick
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */

#include "timebase.h"
#include "project_config.h"
#include "stm32l476xx.h"

#define SYSTEM_CLOCK_HZ          PROJECT_CLOCK_HZ
#define SYSTICK_FREQUENCY_HZ     1000UL

static volatile uint32_t systick_ms = 0;

//******************************************************************************************
// Function: init_systick
//
// Purpose:
//      Configure SysTick to generate a 1 ms interrupt
//
// Inputs:
//      None
//
// Output:
//      None
//******************************************************************************************
void init_systick(void)
{
	// Disable SysTick by clearing the CTRL register
	SysTick->CTRL = 0UL;

	// Start the software clock at zero
	systick_ms = 0;

	// Set SysTick for a 1 ms interrupt
	// System clock = 80 MHz
	// Reload = (80,000,000 / 1000) - 1 = 79,999
	SysTick->LOAD = (SYSTEM_CLOCK_HZ / SYSTICK_FREQUENCY_HZ) - 1UL;

	// Clear the current SysTick counter value
	SysTick->VAL = 0UL;

	// Keep SysTick below B1, sensor capture and UART in interrupt priority
	NVIC_SetPriority(SysTick_IRQn, 3);

	// Use the processor clock for SysTick
	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;

	// Enable the interrupt bit
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;

	// Set the enable bit to start the timer
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
}

//******************************************************************************************
// Function: SysTick_Handler
//
// Purpose:
//      Maintain a 1ms system timing reference
//
// Inputs:
//      None
//
// Output:
//      None
//******************************************************************************************
void SysTick_Handler(void)
{
	systick_ms++;
}

//******************************************************************************************
// Function: timebase_get_ms
//
// Purpose:
//      Read the system timing reference from foreground code
//
// Inputs:
//      None
//
// Output:
//      Current millisecond count
//******************************************************************************************
uint32_t timebase_get_ms(void)
{
	return systick_ms;
}
