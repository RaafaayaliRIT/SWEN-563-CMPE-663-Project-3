/*
 * SysClock.c
 *
 * 	Created on: Jan 21, 2026
 *     	Author: Mitesh Parikh
 *
 * 	Updated for Project 3 on Oct 5, 2026
 * 		Author: Mitesh Parikh
 *
 * 		Clock configuration retained:
 *   		HSI16 = 16 MHz; PLL M = 2, N = 20, R = 2
 *   		SYSCLK, HCLK, PCLK1 and PCLK2 = 80 MHz
 *   		PLLSAI1 P output = (16 MHz / 2) * 24 / 17 = 11.294 MHz
 *
 * 		Changes:
 *   		- Explicitly select voltage range 1 and wait for voltage scaling
 *   		- Use four Flash wait states and verify the latency setting
 *   		- Select HSI before disabling/reconfiguring the PLLs
 *   		- Wait on PLLSAI1RDY rather than PLLSAI1ON
 *   		- Refresh the CMSIS SystemCoreClock value
 *
 * 			Call once during startup, before initializing SysTick, UART or timers.
 * 			PLLSAI2 is assumed to remain disabled, as after reset. This is not a
 * 			runtime clock-switching interface.
 *
 * 			Reference: STM32L476 RM0351 - Flash latency, voltage scaling and RCC
 */

#include "SysClock.h"

//******************************************************************************************
// Switch the PLL source from MSI to HSI, and select the PLL as SYSCLK source.
//******************************************************************************************
void System_Clock_Init(void)
{

	uint32_t HSITrim;

	// Enable the power interface and select voltage range 1 for 80 MHz operation.
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
	(void)RCC->APB1ENR1;
	PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS) | PWR_CR1_VOS_0;
	while((PWR->SR2 & PWR_SR2_VOSF) != 0);

	// To correctly read data from FLASH memory, the number of wait states (LATENCY)
	// must be correctly programmed according to the frequency of the CPU clock
	// (HCLK) and the voltage range. Four wait states are required at 80 MHz.
	FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_4WS;
	while((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_4WS);

	// Enable the Internal High Speed oscillator (HSI
	RCC->CR |= RCC_CR_HSION;
	while((RCC->CR & RCC_CR_HSIRDY) == 0);

	// Adjusts the Internal High Speed oscillator (HSI) calibration value
	// RC oscillator frequencies are factory calibrated by ST for 1 % accuracy at 25oC
	// After reset, the factory calibration value is loaded in HSICAL[7:0] of RCC_ICSCR
	HSITrim = 16; // user-programmable trimming value that is added to HSICAL[7:0] in ICSCR.
	RCC->ICSCR &= ~RCC_ICSCR_HSITRIM;
	RCC->ICSCR |= HSITrim << 24;

	// Select HSI as SYSCLK before disabling and reconfiguring the PLLs.
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
	while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);

	// Disable both PLLs before changing their shared clock source and divider.
	RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_PLLSAI1ON);
	while((RCC->CR & (RCC_CR_PLLRDY | RCC_CR_PLLSAI1RDY)) != 0);

	// Select clock source to PLL
	RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLSRC;
	RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSI; // 00 = No clock, 01 = MSI, 10 = HSI, 11 = HSE

	// Make PLL as 80 MHz
	// f(VCO clock) = f(PLL clock input) * (PLLN / PLLM) = 16MHz * 20/2 = 160 MHz
	// f(PLL_R) = f(VCO clock) / PLLR = 160MHz/2 = 80MHz
	RCC->PLLCFGR = (RCC->PLLCFGR & ~RCC_PLLCFGR_PLLN) | 20U << 8;
	RCC->PLLCFGR = (RCC->PLLCFGR & ~RCC_PLLCFGR_PLLM) | 1U << 4; // 000: PLLM = 1, 001: PLLM = 2, 010: PLLM = 3, 011: PLLM = 4, 100: PLLM = 5, 101: PLLM = 6, 110: PLLM = 7, 111: PLLM = 8

	RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLR;  // 00: PLLR = 2, 01: PLLR = 4, 10: PLLR = 6, 11: PLLR = 8
	RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN; // Enable Main PLL PLLCLK output

	// The maximum frequency of the AHB, APB1 and APB2 domains is 80 MHz.
	RCC->CFGR &= ~RCC_CFGR_HPRE; // AHB prescaler = 1; SYSCLK not divided
	RCC->CFGR &= ~RCC_CFGR_PPRE1; // APB1 prescaler = 1; HCLK not divided
	RCC->CFGR &= ~RCC_CFGR_PPRE2; // APB2 prescaler = 1; HCLK not divided

	RCC->CR   |= RCC_CR_PLLON;
	while((RCC->CR & RCC_CR_PLLRDY) == 0);

	// Select PLL selected as system clock
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL; // 00: MSI, 01:HSI, 10: HSE, 11: PLL

	// Wait until System Clock has been selected
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

	// Configure and enable PLLSAI1 clock to generate 11.294MHz
	// 8 MHz * 24 / 17 = 11.294MHz
	// f(VCOSAI1 clock) = f(PLL clock input) *  (PLLSAI1N / PLLM)
	// PLLSAI1CLK: f(PLLSAI1_P) = f(VCOSAI1 clock) / PLLSAI1P
	// PLLUSB2CLK: f(PLLSAI1_Q) = f(VCOSAI1 clock) / PLLSAI1Q
	// PLLADC1CLK: f(PLLSAI1_R) = f(VCOSAI1 clock) / PLLSAI1R
	RCC->PLLSAI1CFGR &= ~RCC_PLLSAI1CFGR_PLLSAI1N;
	RCC->PLLSAI1CFGR |= 24U<<8;

	// SAI1PLL division factor for PLLSAI1CLK
	// 0: PLLSAI1P = 7, 1: PLLSAI1P = 17
	RCC->PLLSAI1CFGR |= RCC_PLLSAI1CFGR_PLLSAI1P;
	RCC->PLLSAI1CFGR |= RCC_PLLSAI1CFGR_PLLSAI1PEN;

	// SAI1PLL division factor for PLL48M2CLK (48 MHz clock)
	// RCC->PLLSAI1CFGR &= ~RCC_PLLSAI1CFGR_PLLSAI1Q;
	// RCC->PLLSAI1CFGR |= U<<21;
	// RCC->PLLSAI1CFGR |= RCC_PLLSAI1CFGR_PLLSAI1QEN;

	// PLLSAI1 division factor for PLLADC1CLK (ADC clock)
	// 00: PLLSAI1R = 2, 01: PLLSAI1R = 4, 10: PLLSAI1R = 6, 11: PLLSAI1R = 8
	// RCC->PLLSAI1CFGR &= ~RCC_PLLSAI1CFGR_PLLSAI1R;
	// RCC->PLLSAI1CFGR |= U<<25;
	// RCC->PLLSAI1CFGR |= RCC_PLLSAI1CFGR_PLLSAI1REN;

	RCC->CR |= RCC_CR_PLLSAI1ON;  // SAI1 PLL enable
	while((RCC->CR & RCC_CR_PLLSAI1RDY) == 0);

	// SAI1 clock source selection
	// 00: PLLSAI1 "P" clock (PLLSAI1CLK) selected as SAI1 clock
	// 01: PLLSAI2 "P" clock (PLLSAI2CLK) selected as SAI1 clock
	// 10: PLL "P" clock (PLLSAI3CLK) selected as SAI1 clock
	// 11: External input SAI1_EXTCLK selected as SAI1 clock
	RCC->CCIPR &= ~RCC_CCIPR_SAI1SEL;

	RCC->APB2ENR |= RCC_APB2ENR_SAI1EN;

	// Update the CMSIS system clock value after changing the clock configuration.
	SystemCoreClockUpdate();
}


