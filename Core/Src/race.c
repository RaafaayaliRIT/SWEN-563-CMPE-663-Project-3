/*
 * race.c
 * Application for phases 3 through 5
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */

#include "race.h"
#include "project_config.h"
#include "keyboard.h"
#include "servo.h"
#include "ultrasonic.h"
#include "timer.h"
#include "LED.h"
#include "stm32l476xx.h"
#include "timebase.h"

// Choose your car states, events and application data here
// Use your state diagram and transition table to guide the implementation
// The screen snapshot below is only a starting point for showing your work
static RaceDisplay_Snapshot s_view;

//******************************************************************************************
// Initialize your application before interrupts can call it
// Think about the starting state and which values belong to race progress
//******************************************************************************************
void Race_Init(void)
{
	s_view = (RaceDisplay_Snapshot){0};
	s_view.state_text = "PHASE 3 - DRIVING TODO";
	s_view.servo_text = "NOT IMPLEMENTED";
	s_view.sensor_result = ULTRASONIC_NOT_READY;
	s_view.last_key_text = "--";
	// Add your car initialization here
}

//******************************************************************************************
// Service the car in small steps and return to the main loop
// Keep keyboard, sensor, servo and button work moving together
//******************************************************************************************
void Race_Service(uint32_t now_ms)
{
	(void)now_ms;
	Keyboard_Event input;
	for (uint32_t i = 0; i < PROJECT_INPUT_BUDGET && Keyboard_TakeEvent(&input); i++)
	{
		s_view.last_key_text = Keyboard_CommandName(input.command);
		s_view.last_key_ms = input.timestamp_ms;
		// Phase 3 - decide what this command means in your current state
		// The timestamp belongs to reception, so queued input may be old
		// Decide what your application should do with KEY_INPUT_ERROR
	}

	// Phase 3 - service measurements and apply the proximity policy
	// Use physical distance for braking and keep display scaling separate

	// Phase 3 - advance the virtual car using elapsed time
	// Stop at each corner until the correct turn is received
	// Decide how to detect the absence of Up repeats
	// Accelerate only virtual movement and drive time, not hardware deadlines

	// Phase 3 - choose the servo behavior from your application state
	// Reuse your tested sweep, then add the stuck and one-time finish behavior

	// Phase 4 - handle B1 requests and rearm after a stable release
	// Keep progress on resume and require a fresh Up command
	// An obstacle or a required turn still applies after the manual stop clears

	// Phase 5 - update the LED from your state and real elapsed time
	// Keep blinking out of the ISR and turn the LED off at finish
}

//******************************************************************************************
// Record a B1 request and leave the car decision to the foreground
// Holding the button or contact bounce should not count as several presses
//******************************************************************************************
void B1_EXTI_Callback(uint16_t pin)
{
	(void)pin;
	// Phase 4 - add your short interrupt callback here
}

//******************************************************************************************
// Show your application data without exposing it to the renderer
// Populate s_view or replace this copy with an adapter for your own structure
//******************************************************************************************
void Race_GetSnapshot(RaceDisplay_Snapshot *view)
{
	if (view)
		*view = s_view;
}
