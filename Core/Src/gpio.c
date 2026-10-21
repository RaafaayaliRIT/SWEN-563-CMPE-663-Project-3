/*
 * gpio.c
 *
 *  Created on: Nov 4, 2021
 *      Author: Mitesh Parikh
 *
 *      Update on: Jan 21, 2026
 *      Author: Mitesh Parikh
 *      Comments: Translated into Bare metal code, removed use of HAL functions, added references
 *
 *      Updated on: Oct 5, 2026
 *      Comments: Removed Project 2 pulse input; B1 interrupt requests emergency stop
 */


//	References:
//		1. STM32L476 Reference Manual (RM0351)
//				refer to RCC, SYSCFG, EXTI and NVIC info
//		2. NUCLEO-L476RG User Manual (UM1724)
//				refer for Button B1 / PC13 wiring and external pull-ups/Pull-down info
//
//
//	Newer version is implementing Bare-metal EXTI configuration for NUCLEO-L476RG user button B1 (PC13).
//
//	Button wiring note (NUCLEO typical):
//	- B1 is active-low: when pressed, PC13 reads 0.
//	- Therefore we trigger on FALLING edge (1 -> 0).
//
//	EXTI overview:
//	- EXTI line number matches the pin number (PC13 -> EXTI13).
//	- But we must also tell SYSCFG which PORT feeds that EXTI line:
//	  SYSCFG_EXTICR4.EXTI13 = Port C


#include "gpio.h"
#include "stm32l476xx.h"
#include <stdbool.h>

// For Project 3 - race.c provides this callback for B1 stop and resume
extern void B1_EXTI_Callback(uint16_t pin);

// Per User Manual, B1 Switch (Blue Button) is connected by PC13
// PC13 = GPIO Port C, Pin 13
#define B1_GPIO_PORT	GPIOC
#define B1_PIN          13U

//
// Refer to Section 14.5 in RM0351 Rev 9 EXTI registers
// You have to have interupt mask on desired line to generate interrupt
//
//	Interrupt mask register 1 (EXTI_IMR1):
//		Bits 31:0 IMx: Interrupt Mask on line x (x = 31 to 0)
//			0: Interrupt request from Line x is masked
//			1: Interrupt request from Line x is not masked
//
#define B1_EXTI_LINE    (1U << B1_PIN)

// Port encoding for SYSCFG EXTICR fields:
// From Reference Manual RM0351 Rev 9 (pg. 321)
//	0000: PA[13] pin		-- GPIO Port A, Pin 13
//	0001: PB[13] pin		-- GPIO Port B, Pin 13
//	0010: PC[13] pin		-- GPIO Port C, Pin 13
//	0011: PD[13] pin
//	0100: PE[13] pin
//	0101: PF[13] pin
//	0110: PG[13] pin
//	0111: PH[13] pin
//	1000: Reserved

#define SYSCFG_PORTC_CODE  0x2U		// 0010

//******************************************************************************************
// Initialize GPIO and EXTI for the user push-button (B1 on PC13)
//******************************************************************************************
void GPIO_Init(void)
{

    // ------------------------------------------------------------------------
    // Step 1: Enable peripheral clocks
    // ------------------------------------------------------------------------

	// Enable GPIOC clock (button is on PC13)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;

    // Enable SYSCFG clock (needed to route EXTI line to port C)
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // ------------------------------------------------------------------------
    // Step 2:
    //		Configure PC13 as input (default after reset) -- GPIO MODER setting
    //    	Optional pull-up/down:
    //      	Many NUCLEO boards have external circuitry; leaving as "no pull"
    //      	keeps behavior consistent with the board’s default wiring.
    // ------------------------------------------------------------------------
    GPIOC->MODER &= ~(3U << (13U * 2U));
    GPIOC->PUPDR &= ~(3U << (2U * B1_PIN)); // 00 = no pull-up/down

    // ------------------------------------------------------------------------
    // Step 3:
    //		Route EXTI13 to Port C using SYSCFG_EXTICR4
    //
    //    	EXTICR index mapping:
    //      	EXTICR[0] controls EXTI0..3
    //      	EXTICR[1] controls EXTI4..7
    //      	EXTICR[2] controls EXTI8..11
    //      	EXTICR[3] controls EXTI12..15  <-- EXTI13 lives here
    //
    //    	Field position:
    //      	EXTI13 uses bits [7:4] inside EXTICR4 (i.e., EXTICR[3]).
    // ------------------------------------------------------------------------
    SYSCFG->EXTICR[3] &= ~(0xFU << 4);  // clear EXTI13 field
    SYSCFG->EXTICR[3] |=  (SYSCFG_PORTC_CODE << 4);  // 0x2 = Port C

    // ------------------------------------------------------------------------
    // Step 4:
    //		Configure EXTI line 13
    //    		- Unmask interrupt line in IMR1
    //    		- Select falling edge trigger (button press)
    // ------------------------------------------------------------------------
    EXTI->IMR1  |=  (1U << B1_PIN);     // unmask interrupt
    EXTI->EMR1  &= ~(1U << B1_PIN);     // no event
    EXTI->RTSR1 &= ~(1U << B1_PIN);     // disable rising edge
    EXTI->FTSR1 |=  (1U << B1_PIN);     // enable falling edge

    // ------------------------------------------------------------------------
    // Step 5: Clear any pending EXTI interrupt flag on line 13 (write 1 to clear)
    // ------------------------------------------------------------------------
    EXTI->PR1 = (1U << B1_PIN);

    // ------------------------------------------------------------------------
    // Step 6: Enable NVIC interrupt for EXTI lines 10..15
    // ------------------------------------------------------------------------
    NVIC_SetPriority(EXTI15_10_IRQn, 0); // Project 3: highest configurable priority for B1.
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

//******************************************************************************************
// This function is to handle GPIO Extended Interrupt
//******************************************************************************************
void EXTI15_10_IRQHandler(void)
{
	// Check whether B1 caused the interrupt
	// Clear its pending flag and let race.c handle the request
	// See EXTI_PR1 in the reference manual
	if (EXTI->PR1 & (1U << B1_PIN))
    {
        EXTI->PR1 = (1U << B1_PIN);          // clear pending by writing 1
        B1_EXTI_Callback((uint16_t)B1_PIN);  // notify application layer
    }
}
