/*
 * servo.c
 * Phase 1 - one servo to be used only, on D5 / PB4
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */
#include "servo.h"
#include "project_config.h"
#include "stm32l476xx.h"

// RCC Register Masks
#define GPIOBEN_MASK (0x00000002)
#define TIM3EN_MASK (0x00000002)

// Timer Register Masks
#define CEN_MASK (0x0001)
#define CC1E_MASK (0x0001)
#define CC1IE_MASK (0x0002)
#define SMS_MASK (0x000F)

// Timer values
#define TIMER_FREQUENCY_HZ (1000000)
#define FRAME_LENGTH (20000)

//******************************************************************************************
// Set up the servo output on PB4 / TIM3_CH1
// Work out the prescaler, frame length and starting pulse from the timer clock
// Use the reference manual to choose the GPIO and timer settings
// Keep the output off until the starting values are ready
//******************************************************************************************
void Servo_Init(void)
{
	// Add your initialization here

	// Frame length = 20ms
	// PS = 79 - Don't change
	// ARR = 19,999 - Don't change
	// CCR1 := high time in us, don't use -1 like PS/ARR, needs 1000us-2000us for the servo
	// GPIO output should use AF2 (0010)
	// gpio moder 10 for pb4




	// 1. EN GPIOB, TIM3
	// RCC_AHB2RSTR bit 1 for GPIOBEN, write 1
	RCC->AHB2RSTR |= GPIOBEN_MASK;

	// RCC_AHB2ENR bit 1 for TIM3EN, write 1
	RCC->AHB2ENR |= TIM3EN_MASK;

	// 2. Stop counting and disable channel outputs
	TIM3->CR1 &= ~(CEN_MASK); // Disable counter
	TIM3->CCER &= ~(CC1E_MASK); // Disable capture compare for channel 1
	TIM3->DIER &= ~(CC1IE_MASK); // Disable DMA interrupt for channel 1
	TIM3->SMCR &= ~(SMS_MASK); // Disable slave mode

	// 3. Prepare time base, PS/ARR/CNT
	TIM3->PSC = (PROJECT_CLOCK_HZ - TIMER_FREQUENCY_HZ) / TIMER_FREQUENCY_HZ;
	TIM3->ARR = (FRAME_LENGTH * TIMER_FREQUENCY_HZ) - 1;
	TIM3->CNT = 0;

	// 4. Prepare CH1 for TIM3 as PWM mode 1, EN CCR1/ARR preload, CCR1 = 1000

	// 5. Load starting values, ERG.UG=1 then clear startup flags

	// 6. Prepare PB4 route and driver, AF2/Pushpull/Medium speed/no pulls, MODER4=10
	// look at doc

	// 7. EN Channel with active high, CC1E=1, CC1P=0, CC1N=0

	// 8. CEN to start counting last, CR1 bit 0







}

//******************************************************************************************
// Change the pulse width without changing the frame length
// Check the request before touching the timer
// Use preload so a new pulse starts cleanly at a frame boundary
//******************************************************************************************
bool Servo_SetPulseUs(uint32_t pulse_us)
{
	(void)pulse_us; // Remove this when you use the argument
	// Add your pulse update here
	return false; // This starter does not drive the servo yet
}

//******************************************************************************************
// Map one of the six positions to a pulse width
// Think about the two endpoints and what happens for an invalid position
//******************************************************************************************
bool Servo_SetPosition(uint8_t position)
{
	(void)position; // Remove this when you use the argument
	// Add your position mapping here
	return false;
}
