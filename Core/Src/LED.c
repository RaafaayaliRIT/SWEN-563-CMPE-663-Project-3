/*
 * LED.c
 *
 *  Updated on: Sept 9, 2025
 *      Author: Mitesh Parikh
 *
 *  Updated on: Jan 21, 2026
 *  Author: Mitesh Parikh
 *  COmments: Removed unnecessary functions, improved bare-metal code, comments
 */

//	References:
//	1. STM32L476 Reference Manual (RM0351)
//		- RCC AHB2 peripheral clock enable register (GPIOA enable)
//		- GPIO port mode register (GPIOx_MODER)
//		- GPIO output type register (GPIOx_OTYPER)
//		- GPIO output speed register (GPIOx_OSPEEDR)
//		- GPIO pull-up/pull-down register (GPIOx_PUPDR)
//		- GPIO port output data register (GPIOx_ODR)
//		- GPIO port bit set/reset register (GPIOx_BSRR)
//
//	2. NUCLEO-L476RG User Manual (UM1724)
//		- User LED (LD2) connection to PA5
//		- Electrical characteristics of the LED circuit

#include "LED.h"
// CMSIS device header (for register definitions)
#include "stm32l476xx.h"


//******************************************************************************************
// User LED = LD2 Green LED = PA.5 => GPIO Port A, Pin 5, would need mask as well
//******************************************************************************************
#define LED_PIN 		5
#define LED_MASK        (1U << LED_PIN)

//******************************************************************************************
// Initialize GPIO for the on-board user LED (LD2 on PA5)
//******************************************************************************************
void LED_Init(void)
{
	// Enable GPIOA peripheral clock (RCC AHB2)
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

	// GPIO Mode: Input(00), Output(01), AlterFunc(10), Analog(11, reset)
	// Configure PA5 as a general-purpose output (MODER5 = 01)
	//
	GPIOA->MODER &= ~(3U << (2U * LED_PIN));
	GPIOA->MODER |=  (1U << (2U * LED_PIN));    // Output(01)

	// GPIO Speed: Low speed (00), Medium speed (01), Fast speed (10), High speed (11)
	// Setting to High speed => 11 or 3U

	GPIOA->OSPEEDR &= ~(3U << (2U * LED_PIN));
	GPIOA->OSPEEDR |=  (3U << (2U * LED_PIN));  // High speed

	// GPIO Output Type: Output push-pull (0, reset), Output open drain (1)
	// Set Output type to push-pull
	GPIOA->OTYPER &= ~(1U << LED_PIN);       	// Push-pull

	// GPIO Push-Pull: No pull-up, pull-down (00), Pull-up (01), Pull-down (10), Reserved (11)
	// Set to No pull-up / pull-down (PUPDR5 = 00)
	GPIOA->PUPDR  &= ~(3U << (2U * LED_PIN));  	// No pull-up, no pull-down

	// Reset or Turn off LED -- better to start in known state
	LED_Off();
}

//******************************************************************************************
// Turn LED On
//******************************************************************************************
void LED_On(void)
{
	GPIOA->ODR |= LED_MASK;
}

//******************************************************************************************
// Turn LED Off
//******************************************************************************************
void LED_Off(void)
{
	GPIOA->ODR &= ~LED_MASK;
}

//******************************************************************************************
// Toggle LED
//******************************************************************************************
void LED_Toggle(void)
{
	GPIOA->ODR ^= LED_MASK;
}

//******************************************************************************************
// LED Status - Return true if the LED is ON, false otherwise
//******************************************************************************************
bool LED_IsOn(void )
{
	uint32_t bit = GPIOA->ODR & LED_MASK;

	return (bit != 0U);
}
