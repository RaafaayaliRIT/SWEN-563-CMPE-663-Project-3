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
#include "LED.h"

//const uint32_t POSITION_INCREMENT = ((SERVO_MAX_US-SERVO_MIN_US) / SERVO_LAST_POSITION);
//const uint16_t POSITIONS[] =
//{
//	SERVO_MIN_US,
//	SERVO_MIN_US + POSITION_INCREMENT * 1,
//	SERVO_MIN_US + POSITION_INCREMENT * 2,
//	SERVO_MIN_US + POSITION_INCREMENT * 3,
//	SERVO_MIN_US + POSITION_INCREMENT * 4,
//	SERVO_MAX_US
//};

// Sweep direction
typedef enum
{
	SWEEP_UP = 0,
	SWEEP_DOWN // this is equal to 1
} SweepDirection;


// keep this static so only this file has access to these variables, also kep the naming convention from the s_view
static RaceDisplay_Snapshot s_view; // this was the struct we need to use to update the display
static bool s_sweep_started = false;
static uint32_t s_last_step_ms = 0;
static uint8_t s_position = 0;
static SweepDirection s_direction = SWEEP_UP;
static bool s_servo_ok = true; // false = error state;


static bool s_sensor_started = false;  // false until the first request
static uint32_t s_last_request_us = 0; // when the last measurement was started


//int64_t last_time = -1;
//uint8_t current_position = 0;
//uint8_t direction = 0; // 0 for up, 1 for down
//bool isSuccessful = true;

// Add the test data you need for your sweep and sensor checks
// You can use these exercises or test directly in your car application
// There is one final demo of the integrated solution



//******************************************************************************************
// Request one servo position and update the display and sweep state
//******************************************************************************************
static void Sweep_Request(uint8_t position, uint32_t now_ms)
{
	if (Servo_SetPosition(position))
	{
		//save the position and reverse at an endpoint
		s_position = position;


		if(s_position >= SERVO_LAST_POSITION){
			s_direction = SWEEP_DOWN;
		}else if(s_position == 0){
			s_direction = SWEEP_UP;
		}

		//The display fields were never updated, so the screen kept
		//The guide says to set these only
		//after the driver accepts the command.
		s_view.servo_text = "SWEEP";
		s_view.servo_position = s_position;
		s_view.servo_position_valid = true;
	}
	else
	{
		// when it's in the error state
		//hardware keeps the last valid pulse; stop new requests
		s_servo_ok = false;
		s_view.servo_text = "SERVO ERROR"; // HOW DO WE PRINT "SERVO ERROR" <-- THIS IS HOW
		s_view.servo_position_valid = false;
		LED_On(); // added this since we had it
	}

	s_last_step_ms = now_ms;
}






//******************************************************************************************
// Start an exercise with a clear screen snapshot
// Add your own test initialization as you implement the modules // we didn't do this step
//******************************************************************************************
void PhaseTests_Init(void)
{
	s_view = (RaceDisplay_Snapshot){0};
	s_view.state_text = (PROJECT_PHASE == 1U) ? "PHASE 1 - SERVO COMPLETED!" : "PHASE 2 - SENSOR COMPLETED!";
	s_view.servo_text = "NOT IMPLEMENTED";
	s_view.sensor_result = ULTRASONIC_NOT_READY;
	s_view.last_key_text = "--";
	// Add your test initialization here

	// Start of the sweep: position 0, direction UP
	s_sweep_started = false;
	s_last_step_ms = 0;
	s_position = 0;
	s_direction = SWEEP_UP;
	s_servo_ok = true;


	// Start of the sensor exercise: first request goes out on the first pass
	// Ultrasonic_Init is expected to be called from main after timer_init
	s_sensor_started  = false;
	s_last_request_us = 0;
}




//******************************************************************************************
// Test your modules without needing a car state machine yet
// Keyboard activity is shown so you can see whether the loop stays responsive
//******************************************************************************************
void PhaseTests_Service(uint32_t now_ms)
{
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

	/*
	 * NOTE:
	 * I'm not getting into because honestly I forgot how roughly our old code works but I wrote down the old sequence
	 * 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 1, 2, 3 ....
	 * the second time it hit 1, the position was held for 500ms instead of 250, there were a lot of other issues, (Mainly readable and us over complicating it) but over all we weren't far off.
	 * this isn't what we wanted so I just decided to gut the thing and start from scratch since it was so hard trying to fix it.
	 * */

	// If the first call
	if(!s_sweep_started)
	{
		//isSuccessful = Servo_SetPosition(current_position);
		//last_time = now_ms;
		// First call: request position 0 so the display shows SWEEP right away
		s_sweep_started = true;
		Sweep_Request(0, now_ms);
	}
	// Next sweep step after the dwell
	else if(s_servo_ok && (uint32_t)(now_ms - s_last_step_ms) >= PROJECT_SERVO_STEP_MS)
	{
		uint8_t next;
		if(s_direction == SWEEP_UP){
			next = s_position + 1; // don't ++ it's a pre-increment, it would increment before the program checks if it's even valid causing a mismatch between the state and the real position of the servo
		}else{
			// if s_direction == SWEEP_DOWN
			next = s_position - 1; // same thing here
		}
		Sweep_Request(next,now_ms);
	}

#if PROJECT_PHASE >= 2
	// since now_ms isn't small enough
	uint32_t now_us = timer_get_us();

	//Call Ultrasonic_Service every pass
	Ultrasonic_Service(now_us);

	// If the sample interval has elapsed and the sensor is idle

	//IDLE: no measurement in progress and the last result has been taken
	//first request goes right away(!s_sensor_started), after that we wait the ULTRASONIC_INTERVAL_US from the start of the prev request to make sure
	// the attempts are evenly spaced
	if(GetState() == ULTRASONIC_IDLE_STATE && (!s_sensor_started || timer_elapsed_us(s_last_request_us, now_us) >= ULTRASONIC_INTERVAL_US)){
		//Request one measurement/Record the request time if accepted
		//start can still refuse so only record the time if it was accpected
		if(Ultrasonic_Start(now_us))
		{
			s_last_request_us = now_us; // the next interval is measured from here
			s_sensor_started  = true; // from now on, the interval check applies
		}

	}


	Ultrasonic_Sample sample;
	sample.result = ULTRASONIC_NOT_READY;

	// If a new valid sample is available
	if(Ultrasonic_TakeSample(&sample)){
		//Save the distance in the Phase 2 snapshot
		s_view.sensor_result = ULTRASONIC_OK;
		s_view.sensor_distance_mm = sample.distance_mm;
		s_view.sensor_fresh = true;
	}else if(sample.result == ULTRASONIC_TIMEOUT){ //  Else if the measurement timed out
		//Show SENSOR TIMEOUT
		s_view.sensor_result = ULTRASONIC_TIMEOUT;
		s_view.sensor_fresh = true; // false: NO FRESH SAMPLE, true: NO ECHO; also it's range is 2cm-400cm
	}

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
