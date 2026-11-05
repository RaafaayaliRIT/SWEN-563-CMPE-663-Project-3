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
#include "custom_functions.h"


// Route geometry: absolute distance at the end of each section
// (0-40 miles, 40-60 miles, 60-100 miles, from project_config.h)
static const uint32_t s_section_end_mm[3] =
{
	PROJECT_FIRST_CORNER_MILES  * PROJECT_MILE_MM,
	PROJECT_SECOND_CORNER_MILES * PROJECT_MILE_MM,
	PROJECT_TRACK_MILES         * PROJECT_MILE_MM
};

// ****Race display****
static RaceDisplay_Snapshot s_view;

// ****CAR MODEL****
static Car_State s_state;
static const char *s_fault_text;
static uint8_t  s_section; 				// 0 = first straight, 1 = vertical, 2 = last straight
static uint32_t s_position_mm; 			// absolute distance along the route
static uint32_t s_position_rem; 		// leftover (mph * mm * ms) below 1mm, keeps progress smooth
static uint32_t s_speed_mph; 			// 0->100 in 5mph steps
static uint32_t s_drive_time_ms; 		// virtual time spent moving (in-game time)
static uint32_t s_last_step_ms;       	// real time of the last 10ms car update

// ****Up key hold tracking****
// Only Up is held to drive; Down, Left, Right and R act once per press.
// The terminal never reports a key release, so "released" means no Up arrived
// within 750 ms of the first press, or 200ms of the last repeat.
static bool s_hold; 					// Up is considered held
static bool s_hold_repeated; 			// a repeat has arrived since the first press
static uint32_t s_last_up_ms; 			// receive time of the most recent Up
static uint32_t s_last_speed_change_ms;
static uint32_t s_input_floor_ms;		// Up events received before this are old and ignored

//****Sensor****
static bool s_sensor_started;
static uint32_t s_last_request_us;
static bool s_have_result; 				// false until the first result after startup
static uint32_t s_result_us;			// when the latest result completed
static Ultrasonic_Result s_sensor_result;
static uint32_t s_distance_mm;
static bool s_sensor_fresh;				// latest result is younger than 250ms
static bool s_obstacle; 				// fresh valid reading at or below 200mm
static bool s_sensor_inhibit;			// movement not allowed (obstacle, stale, error, none yet)
static bool s_warning; 					// fresh valid reading above from 200mm to 400mm (keep driving, show warning)

//****Servo****
static Servo_Mode s_servo_mode;
static uint8_t s_servo_pos; 			// current commanded position
static uint8_t s_servo_prev;			// previous commanded position (for STUCK)
static bool s_sweep_up;
static bool s_finale_up;
static uint32_t s_servo_last_ms;		// real time of the last position change
static bool s_steer_pending; 			// an accepted turn is waiting to be shown on the servo
static uint8_t s_steer_target; 			// steering position to show (Right = 5, Left = 0)

//****B1 emergency stop (Phase 4)****
static volatile bool s_b1_request;		// set by the B1 interrupt; read and cleared by the foreground
static bool s_paused; 					// B1 emergency stop is active
static bool s_b1_armed;  				// a new press can be accepted (released for 50 ms)
static uint32_t s_b1_last_pressed_ms; 	// last real time B1 was seen pressed

//****LED (Phase 5)****
static Led_Pattern s_led_pattern; 		// pattern currently shown on LD2
static uint32_t s_led_last_ms;			// real time of the last blink toggle


//******************************************************************************************
// HELPER FUNCTIONS
//******************************************************************************************
// B1 is pressed right now (B1 on PC13 reads low when pressed)
static bool B1_IsPressed()
{
	return (GPIOC->IDR & GPIO_IDR_ID13) == 0;
}

// Stop the car and clear the up hold. Up events received before floor_ms are
// treated as old, so a fresh up is needed to drive again
static void Race_StopCar(uint32_t floor_ms)
{
	s_speed_mph = 0;
	s_hold = false;
	s_hold_repeated = false;
	s_input_floor_ms = floor_ms;

	if (s_state == CAR_DRIVING)
	{
		s_state = CAR_READY;
	}
}

// Latch a fault; stop and stay stopped until R (reset)
static void Race_Fault(const char *text, uint32_t now_ms)
{
	Race_StopCar(now_ms);
	s_state = CAR_FAULT;
	s_fault_text = text;
}

// command a servo position and remember the previous one
static void Race_CommandServo(uint8_t position, uint32_t now_ms)
{
	if (Servo_SetPosition(position))
	{
		s_servo_prev = s_servo_pos;
		s_servo_pos = position;
	}
	else
	{
		// The driver keeps the last valid pulse; stop the car
		Race_Fault("SERVO ERROR", now_ms);
	}

	s_servo_last_ms = now_ms;
}

// Start a new race at mile 0, zero speed, servo position 0
static void Race_Restart(uint32_t now_ms)
{

	// Car model: back to a stopped car at mile 0 with no progress and or drive time
	s_state = CAR_READY;
	s_fault_text = "";
	s_section = 0;
	s_position_mm = 0;
	s_position_rem = 0;
	s_speed_mph = 0;
	s_drive_time_ms = 0;
	s_last_step_ms = now_ms;

	// Up key hold tracking: forget any held up; only ups received after this restart count as fresh presses
	s_hold = false;
	s_hold_repeated = false;
	s_last_up_ms = now_ms;
	s_last_speed_change_ms = now_ms;
	s_input_floor_ms = now_ms;

	// Drop old queued input and wait for a short quiet interval
	Keyboard_DiscardPending(now_ms);

	// servo back to 0, no distinct previous position yet
	s_servo_mode = SERVO_MODE_HOLD;
	s_sweep_up = true;
	s_finale_up = true;
	s_servo_pos = 0;
	s_servo_prev = 0;
	s_steer_pending = false;
	s_steer_target = 0;
	Race_CommandServo(0, now_ms);
}

// Up hold release
// Before any repeat: release 750ms after the first Up.
// After repeats start: release 200ms after the last Up
// Checked with each event's receive time and again with the current time, so a gap in old queued input still counts as a release.
static void Race_CheckRelease(uint32_t check_ms)
{
	// nothing to release if Up is not being held
	if (!s_hold)
	{
		return;
	}

	uint32_t limit;
	// Pick the release timeout: shorter once the PC has started repeating Up
	if (s_hold_repeated)
	{
		limit = PROJECT_UP_TIMEOUT_MS;// 200ms
	}
	else
	{
		limit = PROJECT_FIRST_REPEAT_MS; //delay:750ms
	}

	// If no Up arrived within the timeout, the key has been released
	if ((uint32_t)(check_ms - s_last_up_ms) > limit)
	{
		// The key was released at the deadline; anything before it is old
		Race_StopCar(s_last_up_ms + limit);
	}
}

// Add one speed step if allowed.
static void Race_Accelerate(uint32_t event_ms, uint32_t now_ms)
{
	// No movement at a corner (waiting for the turn) or while the sensor inhibits
	//// or during a B1 emergency stop(phase 4)
	if (s_state == CAR_CORNER || s_sensor_inhibit || s_paused)
	{
		return;
	}

	// If the car is starting from rest, start the movement clock now
	if (s_speed_mph == 0)
	{
		s_last_step_ms = now_ms;
	}

	s_speed_mph += PROJECT_SPEED_STEP_MPH;

	// Cap the speed at 100mph
	if (s_speed_mph > PROJECT_MAX_SPEED_MPH)
	{
		s_speed_mph = PROJECT_MAX_SPEED_MPH;
	}

	s_last_speed_change_ms = event_ms;
	s_state = CAR_DRIVING;
}


//******************************************************************************************
// Keyboard: One helper function per command
// t_ms is the Keyboard_Event's original receive time, not the time it's processed
//******************************************************************************************

// Up: start or refresh the hold and speed up (rate limited)
static void Race_HandleUp(uint32_t t_ms, uint32_t now_ms)
{
	// ignore Up after the finish or during a fault; only R recovers
	if (s_state == CAR_FINISHED || s_state == CAR_FAULT)
	{
		return;
	}

	// ignore Up during a B1 emergency stop, B1 must resume first
	if (s_paused)
	{
		return;
	}

	// ignore old Up events received before the last stop
	if (Time_Before(t_ms, s_input_floor_ms))
	{
		return;
	}

	// ignore Up while the path is blocked; a fresh Up after it clears will drive
	if (s_sensor_inhibit)
	{
		return;
	}

	// if up is not already held, this is a fresh press; otherwise it is a repeat
	if (!s_hold)
	{
		// fresh press: start the hold and take the first speed step
		s_hold = true;
		s_hold_repeated = false;
		s_last_up_ms = t_ms;
		Race_Accelerate(t_ms, now_ms);
	}
	else
	{
		// Repeat: always refreshes the hold, speed change is rate limited
		s_hold_repeated = true;
		s_last_up_ms = t_ms;

		// Speed up only if 100ms have passed since the last speed change
		if ((uint32_t)(t_ms - s_last_speed_change_ms) >= PROJECT_SPEED_KEY_MS)
		{
			Race_Accelerate(t_ms, now_ms);
		}
	}
}

// Down: brake to zero and clear the hold
static void Race_HandleDown(uint32_t t_ms)
{
	// nothing to brake after the finish or during a fault
	if (s_state == CAR_FINISHED || s_state == CAR_FAULT)
	{
		return;
	}

	Race_StopCar(t_ms);
}

// Right:take the turn only when waiting at the mile 40 corner
static void Race_HandleRight()
{
	// A wrong or early turn, or a turn during a B1 stop leaves the route unchanged
	if (s_state != CAR_CORNER || s_section != 0 || s_paused)
	{
		return;
	}

	s_section = 1;
	s_state = CAR_READY;// speed is zero, Up (still held or fresh) drives on

	// Steering indication: swing the servo to the Right position
	s_steer_target = RACE_STEER_RIGHT_POS;
	s_steer_pending = true;
}

// Left:take the turn only when waiting at the mile 60 corner
static void Race_HandleLeft()
{
	// A wrong or early turn, or a turn during a B1 stop leaves the route unchanged
	if (s_state != CAR_CORNER || s_section != 1 || s_paused)
	{
		return;
	}

	s_section = 2;
	s_state = CAR_READY;

	// Steering indication: swing the servo to the Left position
	s_steer_target = RACE_STEER_LEFT_POS;
	s_steer_pending = true;
}

//restart, but never bypass a B1 stop or a held/bouncing button
static void Race_HandleRestart(uint32_t now_ms)
{
	// reset is ignored during a B1 stop, press B1 to resume first
	if (s_paused)
	{
		return;
	}

	// reset is ignored while B1 is held or still settling after a press
	if (B1_IsPressed() || !s_b1_armed)
	{
		return;
	}

	Race_Restart(now_ms);
}

// Route one keyboard event to its handler
static void Race_HandleEvent(const Keyboard_Event *ev, uint32_t now_ms)
{
	uint32_t t_ms = ev->timestamp_ms;// use the original timestamp_ms, not now_ms

	// A gap before this event may already have released the Up hold
	Race_CheckRelease(t_ms);

	switch (ev->command)
	{
	case KEY_UP:
		Race_HandleUp(t_ms, now_ms);
		break;

	case KEY_DOWN:
		Race_HandleDown(t_ms);
		break;

	case KEY_RIGHT:
		Race_HandleRight();
		break;

	case KEY_LEFT:
		Race_HandleLeft();
		break;

	case KEY_RESTART:
		// Phase 4: R must not bypass an B1 pause or a held button
		Race_HandleRestart(now_ms);
		//Race_Restart(now_ms);
		break;

	case KEY_INPUT_ERROR:
	default:
		// An input error stops the car and requires a deliberate R restart
		Race_Fault("INPUT ERROR - PRESS R", now_ms);
		break;
	}
}


//******************************************************************************************
// Sensor: keep measurements running and apply the proximity policy
//******************************************************************************************
static void Race_ServiceSensor()
{
	uint32_t now_us = timer_get_us();
	Ultrasonic_Service(now_us);

	// same pattern from phase_test.c for phase2
	if (GetState() == ULTRASONIC_IDLE_STATE && (!s_sensor_started || timer_elapsed_us(s_last_request_us, now_us) >= ULTRASONIC_INTERVAL_US))
	{
		// Record the request time only if the driver accepted the request
		if (Ultrasonic_Start(now_us))
		{
			s_last_request_us = now_us;
			s_sensor_started = true;
		}
	}

	Ultrasonic_Sample sample;
	sample.result = ULTRASONIC_NOT_READY;

	// If a new valid sample is ready, save its distance and completion time
	if (Ultrasonic_TakeSample(&sample))
	{
		s_have_result = true;
		s_sensor_result = ULTRASONIC_OK;
		s_distance_mm = sample.distance_mm;
		s_result_us = sample.timestamp_us;
	}
	// else if the attempt timed out or failed, save that result instead
	else if(sample.result == ULTRASONIC_TIMEOUT || sample.result == ULTRASONIC_ERROR)
	{
		// A timeout result has no completion time stamp in the sample; it finished now
		s_have_result = true;
		s_sensor_result = sample.result;
		s_result_us = now_us;
	}

	// Freshness: a result 250ms old or more is stale for driving
	s_sensor_fresh = (s_have_result && timer_elapsed_us(s_result_us, now_us) < PROJECT_SENSOR_STALE_US);

	// Fresh OK at or below 20cm -> obstacle
	s_obstacle = (s_sensor_fresh && s_sensor_result == ULTRASONIC_OK && s_distance_mm <= PROJECT_OBSTACLE_MM);

	// Fresh OK above 20cm but at or below 40cm -> warning range (keep driving, show warning)
	s_warning = (s_sensor_fresh && s_sensor_result == ULTRASONIC_OK && s_distance_mm > PROJECT_OBSTACLE_MM && s_distance_mm <= RACE_WARNING_MM);

	// A fresh no-echo timeout means no object and may allow driving.
	// No result yet, a stale result or an error inhibits movement.
	bool usable = (s_sensor_fresh &&(s_sensor_result == ULTRASONIC_OK || s_sensor_result == ULTRASONIC_TIMEOUT));

	// Block movement if the sensor can't be trusted right now (no result yet, stale or error) or if it sees an object within 20 cm
	s_sensor_inhibit = (!usable || s_obstacle);
}

//******************************************************************************************
// car model: advance the virtual car in 10ms real steps at 100x virtual time
//******************************************************************************************
static void Race_AdvanceOneStep()
{
	// 10ms real = 1000ms virtual
	const uint32_t virtual_ms = PROJECT_STEP_MS * PROJECT_TIME_SCALE;

	// distance_mm = mph * mm_per_mile * virtual_ms / ms_per_hour
	// 64-bit because top speed is (100 × 1,609,344(one mile in mm) × 1000 = 160,934,400,000); the remainder carries sub-millimeter progress to the next step
	uint64_t num = (uint64_t)s_speed_mph * PROJECT_MILE_MM * virtual_ms + s_position_rem;
	uint32_t delta_mm = (uint32_t)(num/RACE_MS_PER_HOUR);
	s_position_rem = (uint32_t)(num%RACE_MS_PER_HOUR);

	s_drive_time_ms += virtual_ms;

	uint32_t end_mm = s_section_end_mm[s_section];
	// if this step would reach or pass the end of the section, stop exactly there
	if (s_position_mm + delta_mm >= end_mm)
	{
		//exactly to the corner or finish; never carry through a turn
		s_position_mm = end_mm;
		s_position_rem = 0;
		s_speed_mph = 0;

		//end of the last section is the finish; otherwise it is a corner
		if (s_section == 2)
		{
			s_state = CAR_FINISHED;
			s_hold = false;
		}
		else
		{
			// Keep the hold: after the turn, driving can continue on the next up
			s_state = CAR_CORNER;
		}
	}
	else
	{
		s_position_mm += delta_mm; // still short of the end, just move forward
	}
}

static void Race_UpdateCar(uint32_t now_ms)
{

	// The release deadline may have passed with no new event
	Race_CheckRelease(now_ms);

	// If an obstacle, sensor problem or B1 stop appears while driving, stop right away
	if ((s_sensor_inhibit || s_paused) && s_state == CAR_DRIVING)
	{
		Race_StopCar(now_ms);
	}

	// If the car is not driving, there is no movement to apply
	if (s_state != CAR_DRIVING)
	{
		// Not moving: keep the clock current so nothing builds up
		s_last_step_ms = now_ms;
		return;
	}

	uint32_t elapsed = (uint32_t)(now_ms - s_last_step_ms);
	// If the loop was late by more than 50ms, stop
	if (elapsed > RACE_STALL_MS)
	{
		Race_Fault("DRIVE STALL, PRESS R", now_ms);
		return;
	}
	// while at least 10ms of real time is still unprocessed and the car is still driving, move the car one step (1s of virtual time) and use up those 10ms. Stops early if a step reaches a corner or the finish.
	while (elapsed >= PROJECT_STEP_MS && s_state == CAR_DRIVING)
	{
		Race_AdvanceOneStep();
		s_last_step_ms += PROJECT_STEP_MS;
		elapsed -= PROJECT_STEP_MS;
	}
}

//******************************************************************************************
// Servo animation from the car condition, 250ms real dwell
//******************************************************************************************
static Servo_Mode Race_ChooseServoMode()
{
	// Checked in priority order:
	//finished,
	//fault,
	//B1 stop,
	//stuck,
	//moving,
	//hold

	// Finished: run the finale, or stay done if it already ran
	if (s_state == CAR_FINISHED)
	{
		// If the finale already ran, stay done; otherwise run it
		if (s_servo_mode == SERVO_MODE_DONE)
		{
			return SERVO_MODE_DONE;
		}
		return SERVO_MODE_FINALE;
	}

	// fault, hold the current position
	if (s_state == CAR_FAULT)
	{
		return SERVO_MODE_HOLD;
	}

	// B1 emergency stop: hold the current position
	if(s_paused)
	{
		return SERVO_MODE_HOLD;
	}

	// Obstacle or corner stop: alternate between two positions
	if (s_obstacle || s_state == CAR_CORNER)
	{
		return SERVO_MODE_STUCK;
	}

	// moving: sweep
	if (s_state == CAR_DRIVING)
	{
		return SERVO_MODE_SWEEP;
	}

	return SERVO_MODE_HOLD;    // ordinary stop or sensor not ready
}

// Set up a new animation when the mode changes
static void Race_EnterServoMode(Servo_Mode wanted)
{
	// Entering STUCK with no distinct previous position, pick a neighbor
	if (wanted == SERVO_MODE_STUCK && s_servo_prev == s_servo_pos)
	{
		// Use the position above or below if already at 5
		if (s_servo_pos < SERVO_LAST_POSITION)
		{
			s_servo_prev = (uint8_t)(s_servo_pos + 1);
		}
		else
		{
			s_servo_prev = (uint8_t)(s_servo_pos - 1);
		}
	}

	//entering FINALE: go toward 5 first, even if the sweep was heading down.
	// already at 5 -> just return to 0
	if (wanted == SERVO_MODE_FINALE)
	{
		s_finale_up = (s_servo_pos != SERVO_LAST_POSITION);
	}

	s_servo_mode = wanted;
}

//STUCK: alternate between the previous and current positions
static void Race_ServoStuckStep(uint32_t now_ms)
{
	// Commanding the previous position swaps current and previous
	Race_CommandServo(s_servo_prev, now_ms);
}

// SWEEP: one step of 0 1 2 3 4 5 4 3 2 1 0
static void Race_ServoSweepStep(uint32_t now_ms)
{
	// At 5 going up, turn down; at 0 going down, turn up
	if (s_sweep_up && s_servo_pos >= SERVO_LAST_POSITION)
	{
		s_sweep_up = false;
	}
	else if (!s_sweep_up && s_servo_pos == 0)
	{
		s_sweep_up = true;
	}

	// Step one position in the current direction
	if (s_sweep_up)
	{
		Race_CommandServo((uint8_t)(s_servo_pos + 1), now_ms);
	}
	else
	{
		Race_CommandServo((uint8_t)(s_servo_pos - 1), now_ms);
	}
}

//FINALE: one step of "up to 5, then down to 0", run once
static void Race_ServoFinaleStep(uint32_t now_ms)
{
	// Going up: step up and turn around at 5
	if (s_finale_up)
	{
		Race_CommandServo((uint8_t)(s_servo_pos + 1), now_ms);

		// Reached 5 then head back down
		if (s_servo_pos >= SERVO_LAST_POSITION)
		{
			s_finale_up = false;
		}
	}
	else
	{   //// Going down: step down and finish at 0
		Race_CommandServo((uint8_t)(s_servo_pos - 1), now_ms);

		// Reached 0: the finale is done (once only; never restarts)
		if (s_servo_pos == 0)
		{
			s_servo_mode = SERVO_MODE_DONE;
		}
	}
}

// Pick the animation from the car condition and take one step every 250ms
static void Race_UpdateServo(uint32_t now_ms)
{
	Servo_Mode wanted = Race_ChooseServoMode();

	// If the animation is changing, set up the new one
	if (wanted != s_servo_mode)
	{
		Race_EnterServoMode(wanted);
	}

	// Wait 250ms between position changes
	if ((uint32_t)(now_ms - s_servo_last_ms) < PROJECT_SERVO_STEP_MS)
	{
		return;
	}

	// If a turn was just accepted, show the steering position first.
	// The car is stopped after a turn (HOLD), so the servo stays there until it drives.
	if (s_steer_pending)
	{
		s_steer_pending = false;
		Race_CommandServo(s_steer_target, now_ms);
		return;
	}

	switch (s_servo_mode)
	{
	case SERVO_MODE_SWEEP:
		Race_ServoSweepStep(now_ms);
		break;

	case SERVO_MODE_STUCK:
		Race_ServoStuckStep(now_ms);
		break;

	case SERVO_MODE_FINALE:
		Race_ServoFinaleStep(now_ms);
		break;

	case SERVO_MODE_HOLD:
	case SERVO_MODE_DONE:
	default:
		// keep the current position
		break;
	}
}


//******************************************************************************************
// B1 emergency stop and resume (Phase 4)
// The ISR only latches a request; all decisions happen here in the foreground
//******************************************************************************************

// Read and clear the ISR's request.
// Only cleared when it was set, so no interrupt masking is needed: if the ISR
// fires again between the check and the clear, that second edge is just
// merged into the press we are already returning.
static bool Race_TakeB1Request()
{
	// No request waiting
	if (!s_b1_request)
	{
		return false;
	}

	s_b1_request = false;
	return true;
}

// Accepted press: pause if running, resume if paused
static void Race_ToggleB1Stop(uint32_t now_ms)
{

	// Once finished, B1 leaves the car finished and lets the servo finale complete
	if (s_state == CAR_FINISHED)
	{
		return;
	}

	if (!s_paused)
	{
		// Stop: speed 0, clear the Up hold. Position, section and drive time are kept.
		s_paused = true;
		Race_StopCar(now_ms);
	}
	else
	{
		// resume at zero speed, only an Up received after this point drives.
		// An obstacle, sensor fault, latched fault or corner turn still applies.
		s_paused = false;
		s_input_floor_ms = now_ms;
	}

	// Discard drive input queued around the stop or resume
	Keyboard_DiscardPending(now_ms);
}

static void Race_ServiceB1(uint32_t now_ms)
{
	bool pressed = B1_IsPressed();
	bool request = Race_TakeB1Request();   // read and clear any edge the ISR latched

	// Remember the last time B1 was seen pressed (held or bouncing)
	if (pressed)
	{
		s_b1_last_pressed_ms = now_ms;
	}

	// if armed, accept a press from the ISR edge or from the pin level.
	// The level check catches a button already held at startup.
	if (s_b1_armed && (request || pressed))
	{
		s_b1_armed = false;// one press only, until released again
		s_b1_last_pressed_ms = now_ms;
		Race_ToggleB1Stop(now_ms);
		return;
	}

	// If not armed, re-arm only after B1 has been released continuously for 50ms
	if (!s_b1_armed && !pressed && (uint32_t)(now_ms - s_b1_last_pressed_ms) >= PROJECT_BUTTON_RELEASE_MS)
	{
		s_b1_armed = true;
	}
}

//******************************************************************************************
// LED feedback (Phase 5): chosen from the car condition, blinking uses real time
//******************************************************************************************
// pattern was copied from Race_ChooseServoMode()
static Led_Pattern Race_ChooseLedPattern()
{
	// Checked in priority order:
	//finished,
	//B1 stop or fault,
	//obstacle,
	//moving,
	//off

	// Finished: off, even during the servo finale or while B1 is pressed
	if (s_state == CAR_FINISHED)
	{
		return LED_PATTERN_OFF;
	}

	// B1 emergency stop or fault: blink 2 Hz
	if (s_paused || s_state == CAR_FAULT)
	{
		return LED_PATTERN_BLINK_2HZ;
	}

	// automatic obstacle brake: blink 1 Hz
	if (s_obstacle)
	{
		return LED_PATTERN_BLINK_1HZ;
	}

	// MOVING: steady on
	if (s_state == CAR_DRIVING)
	{
		return LED_PATTERN_ON;
	}

	return LED_PATTERN_OFF;    // ready, ordinary stop or corner wait
}



static void Race_UpdateLed(uint32_t now_ms)
{
	Led_Pattern wanted = Race_ChooseLedPattern();

	// If the condition changed, show the new output right away;
	// a blinking pattern starts with the LED on
	if (wanted != s_led_pattern)
	{
		s_led_pattern = wanted;
		s_led_last_ms = now_ms;

		if (wanted == LED_PATTERN_OFF){
			LED_Off();
		}else{
			LED_On();
		}
		return;
	}

	// Pick the half period for a blinking pattern; steady patterns have nothing to do
	uint32_t half_ms;
	if (s_led_pattern == LED_PATTERN_BLINK_1HZ)
	{
		half_ms = RACE_LED_1HZ_HALF_MS;// 500ms
	}
	else if (s_led_pattern == LED_PATTERN_BLINK_2HZ)
	{
		half_ms = RACE_LED_2HZ_HALF_MS; // 250ms
	}
	else
	{
		return;
	}

	// Toggle once each half period of real time
	if ((uint32_t)(now_ms - s_led_last_ms) >= half_ms)
	{
		if (LED_IsOn())
		{
			LED_Off();
		}
		else
		{
			LED_On();
		}
		s_led_last_ms = now_ms;
	}
}



//******************************************************************************************
// Fill the screen snapshot from the application data
//******************************************************************************************
static const char *Race_StateText()
{
	// Show the highest-priority condition first
	if (s_state == CAR_FAULT)
	{
		return s_fault_text;
	}

	if (s_state == CAR_FINISHED)
	{
		return "FINISHED";
	}


	if(s_paused)
	{
		return "B1 STOP - PRESS B1";
	}

	if (s_obstacle)
	{
		return "OBSTACLE BRAKE";
	}

	if (s_sensor_inhibit)
	{
		return "SENSOR NOT READY";
	}

	if (s_state == CAR_CORNER)
	{
		// Mile 40 needs Right, mile 60 needs Left
		if (s_section == 0)
		{
			return "CORNER - PRESS RIGHT";
		}
		else
		{
			return "CORNER - PRESS LEFT";
		}

	}

	// Object in the warning range, still allowed to drive
	if (s_warning)
	{
		return "OBSTACLE WARNING";
	}


	if (s_state == CAR_DRIVING)
	{
		return "DRIVING";
	}


	return "READY";
}

static const char *Race_ServoText()
{
	switch (s_servo_mode)
	{
	case SERVO_MODE_SWEEP:
		return "SWEEP";

	case SERVO_MODE_STUCK:
		return "STUCK";

	case SERVO_MODE_FINALE:
		return "FINALE";

	case SERVO_MODE_DONE:
		return "DONE";

	case SERVO_MODE_HOLD:
	default:
		// HOLD, plus a fall back for any unexpected value
		return "HOLD";
	}
}

static void Race_UpdateSnapshot()
{
	// Car: state text, speed, progress and drive time for the top status lines
	// and the car marker on the map. Speed is stored in mph and converted to
	// mm/s here, because the display expects mm/s.
	s_view.state_text = Race_StateText();
	s_view.velocity_mm_s = (uint32_t)(((uint64_t)s_speed_mph * PROJECT_MILE_MM) / 3600U);
	s_view.position_mm = s_position_mm;
	s_view.drive_time_ms = s_drive_time_ms;
	s_view.section = s_section;

	// Servo: the current animation name and commanded position for the
	//"Servo:" line. Always valid, since Race_Restart commands position 0.
	s_view.servo_text = Race_ServoText();
	s_view.servo_position = s_servo_pos;
	s_view.servo_position_valid = true;

	// Sensor: freshness, result, physical distance and obstacle flag for the
	// "Obstacle / Sim / Sensor" line. The renderer applies the display scale;
	// braking decisions already used the physical mm value.
	s_view.sensor_fresh = s_sensor_fresh;
	s_view.sensor_result = s_sensor_result;
	s_view.sensor_distance_mm = s_distance_mm;
	s_view.obstacle = s_obstacle;
}

//******************************************************************************************
// Initialize your application before interrupts can call it
// Think about the starting state and which values belong to race progress
//******************************************************************************************
void Race_Init(void)
{
	//s_view = (RaceDisplay_Snapshot){0};
	//s_view.state_text = "PHASE 3 - DRIVING TODO";
	//s_view.servo_text = "NOT IMPLEMENTED";
	//s_view.sensor_result = ULTRASONIC_NOT_READY;
	//s_view.last_key_text = "--";

	// Add your car initialization here

	uint32_t now_ms = timebase_get_ms();

	s_view = (RaceDisplay_Snapshot){0};
	s_view.sensor_result = ULTRASONIC_NOT_READY;
	s_view.last_key_text = "--";

	// sensor starts with no result, so movement is inhibited until one arrives
	s_sensor_started = false;
	s_last_request_us = 0;
	s_have_result = false;
	s_result_us = 0;
	s_sensor_result = ULTRASONIC_NOT_READY;
	s_distance_mm = 0;
	s_sensor_fresh = false;
	s_obstacle = false;
	s_sensor_inhibit = true;
	s_warning = false;

	// B1: not paused, armed. A button held at startup is caught by the (phase 4)
	// level check in Race_ServiceB1 on the first pass.
	s_b1_request = false;
	s_paused = false;
	s_b1_armed = true;
	s_b1_last_pressed_ms = now_ms;

	// LED: start off (phase 5)
	s_led_pattern = LED_PATTERN_OFF;
	s_led_last_ms = now_ms;
	LED_Off();

	//car stopped at mile 0, servo at position 0
	Race_Restart(now_ms);
	Race_UpdateSnapshot();
}

//******************************************************************************************
// Service the car in small steps and return to the main loop
// Keep keyboard, sensor, servo and button work moving together
//******************************************************************************************
void Race_Service(uint32_t now_ms)
{

	Keyboard_Event input;
	for (uint32_t i = 0; i < PROJECT_INPUT_BUDGET && Keyboard_TakeEvent(&input); i++)
	{
		// Phase 3 - decide what this command means in your current state
		// The timestamp belongs to reception, so queued input may be old
		// Decide what your application should do with KEY_INPUT_ERROR

		s_view.last_key_text = Keyboard_CommandName(input.command);
		s_view.last_key_ms = input.timestamp_ms;
		Race_HandleEvent(&input, now_ms);
	}

	// Phase 4 - handle B1 requests and rearm after a stable release
	// Keep progress on resume and require a fresh Up command
	// An obstacle or a required turn still applies after the manual stop clears
	// runs before the car update, so a press stops the very next movement step
	Race_ServiceB1(now_ms);


	// Phase 3 - service measurements and apply the proximity policy
	// Use physical distance for braking and keep display scaling separate
	Race_ServiceSensor();


	// Phase 3 - advance the virtual car using elapsed time
	// Stop at each corner until the correct turn is received
	// Decide how to detect the absence of Up repeats
	// Accelerate only virtual movement and drive time, not hardware deadlines
	Race_UpdateCar(now_ms);


	// Phase 3 - choose the servo behavior from your application state
	// Reuse your tested sweep, then add the stuck and one-time finish behavior
	Race_UpdateServo(now_ms);



	// Phase 5 - update the LED from your state and real elapsed time
	// Keep blinking out of the ISR and turn the LED off at finish
	Race_UpdateLed(now_ms);

	//Snapshot for the display
	Race_UpdateSnapshot();
}

//******************************************************************************************
// Record a B1 request and leave the car decision to the foreground
// Holding the button or contact bounce should not count as several presses
//******************************************************************************************
void B1_EXTI_Callback(uint16_t pin)
{
	(void)pin;
	// Phase 4 - add your short interrupt callback here
	// Only latch the request; bounce and held presses are filtered in Race_ServiceB1
	s_b1_request = true;
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
