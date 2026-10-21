/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  *
  * Prepared by Mitesh Parikh - Sept 2025
  *
  * Updated by Mitesh Parikh - Sept 2026
  *
  * Updated for Project 3 by Mitesh Parikh - Oct 2026
  *
  ******************************************************************************
  */

/* --------------------
 * Includes
 * --------------------*/
#include "SysClock.h"
#include "uart.h"
#include "timer.h"
#include "gpio.h"
#include "LED.h"
#include "servo.h"
#include "ultrasonic.h"
#include "race.h"
#include "phase_tests.h"
#include "race_display.h"
#include "keyboard.h"
#include "timebase.h"
#include "project_config.h"


//******************************************************************************************
// Default Entry Point
//
// Start the supplied support and the modules for your selected development mode
// Use PROJECT_PHASE in project_config.h to choose an exercise or the car
// Set it to 5 for your finished solution and final demo
//******************************************************************************************
int main(void)
{
	__disable_irq();
	System_Clock_Init();
	LED_Init();
	USART_Init(115200);
	timer_init();
	init_systick();
	Keyboard_Init();
	RaceDisplay_Init();
	Servo_Init();

#if PROJECT_PHASE >= 2
	Ultrasonic_Init();
#endif

#if PROJECT_PHASE >= 3
	Race_Init();
#else
	PhaseTests_Init();
#endif

	GPIO_Init();
	USART2_EnableIRQ();
	__enable_irq();

	while (1)
	{
		uint32_t now_ms = timebase_get_ms();
		Keyboard_Service(now_ms);
		RaceDisplay_Snapshot view;

#if PROJECT_PHASE >= 3
		Race_Service(now_ms);
		Race_GetSnapshot(&view);
#else
		PhaseTests_Service(now_ms);
		PhaseTests_GetSnapshot(&view);
#endif
		RaceDisplay_Service(&view, now_ms);
	}
}
