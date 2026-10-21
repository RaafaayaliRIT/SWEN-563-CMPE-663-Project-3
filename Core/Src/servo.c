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

//******************************************************************************************
// Set up the servo output on PB4 / TIM3_CH1
// Work out the prescaler, frame length and starting pulse from the timer clock
// Use the reference manual to choose the GPIO and timer settings
// Keep the output off until the starting values are ready
//******************************************************************************************
void Servo_Init(void)
{
	// Add your initialization here
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
