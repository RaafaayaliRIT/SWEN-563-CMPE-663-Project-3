/*
 * phase_tests.c
 * Optional exercises to try the servo and sensor on their own
 */
#include "phase_tests.h"
#include "project_config.h"
#include "keyboard.h"
#include "servo.h"
#include "ultrasonic.h"
#include "timer.h"
#include "timebase.h"

const uint32_t POSITION_INCREMENT = (SERVO_MAX_US-SERVO_MIN_US / 6);
const uint16_t POSITIONS[] =
{
	SERVO_MIN_US,
	SERVO_MIN_US + POSITION_INCREMENT * 1,
	SERVO_MIN_US + POSITION_INCREMENT * 2,
	SERVO_MIN_US + POSITION_INCREMENT * 3,
	SERVO_MIN_US + POSITION_INCREMENT * 4,
	SERVO_MAX_US
};

static RaceDisplay_Snapshot s_view;
int64_t last_time = -1;
uint8_t current_position = 0;
uint8_t direction = 0; // 0 for up, 1 for down

// Add the test data you need for your sweep and sensor checks
// You can use these exercises or test directly in your car application
// There is one final demo of the integrated solution

//******************************************************************************************
// Start an exercise with a clear screen snapshot
// Add your own test initialization as you implement the modules
//******************************************************************************************
void PhaseTests_Init(void)
{
	s_view = (RaceDisplay_Snapshot){0};
	s_view.state_text = (PROJECT_PHASE == 1U) ? "PHASE 1 - SERVO TODO" : "PHASE 2 - SENSOR TODO";
	s_view.servo_text = "NOT IMPLEMENTED";
	s_view.sensor_result = ULTRASONIC_NOT_READY;
	s_view.last_key_text = "--";
	// Add your test initialization here
}




//******************************************************************************************
// Test your modules without needing a car state machine yet
// Keyboard activity is shown so you can see whether the loop stays responsive
//******************************************************************************************
void PhaseTests_Service(uint32_t now_ms)
{
	(void)now_ms;
	Keyboard_Event input;
	for (uint32_t i = 0; i < PROJECT_INPUT_BUDGET && Keyboard_TakeEvent(&input); i++)
	{
		s_view.last_key_text = Keyboard_CommandName(input.command);
		s_view.last_key_ms = input.timestamp_ms;
		// Add any keyboard controls you want for your exercise
	}

	// Phase 1 - command all six positions and then repeat the sweep
	// Let the loop return between steps instead of waiting here
	// Update the servo fields only when your driver accepts the command



	bool isSuccessful = true;


	// If the first call
	if(last_time < 0)
	{
		isSuccessful = Servo_SetPosition(current_position);
	}
	// Handle Up counting
	else if(now_ms - last_time > PROJECT_SERVO_STEP_MS && direction == 0)
	{
		// Increase Position
		current_position++;

		// If out of bounds
		if(current_position >= ARRAY_LENGTH(POSITIONS))
		{
			direction = 1;
			current_position -= 2;
		}


		isSuccessful = Servo_SetPosition(current_position);
	}
	// Handle Down counting
	else if(now_ms - last_time > PROJECT_SERVO_STEP_MS && direction == 1)
	{
		// Decrease Position
		current_position--;

		// If out of bounds
		if(current_position == 0)
		{
			direction = 0;
			current_position++;
		}

		isSuccessful = Servo_SetPosition(current_position);
	}

	if(!isSuccessful)
	{
		isSuccessful = true;
	}

	last_time = now_ms;





#if PROJECT_PHASE >= 2
	// Phase 2 - keep the sweep running while you measure distance
	// Service the sensor, start attempts when allowed and take new results
	// Show the physical reading and whether it is fresh in s_view
#endif
}

//******************************************************************************************
// Give the supplied renderer a copy of the exercise data
//******************************************************************************************
void PhaseTests_GetSnapshot(RaceDisplay_Snapshot *view)
{
	if (view)
		*view = s_view;
}
