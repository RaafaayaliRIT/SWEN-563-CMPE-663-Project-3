/*
 * ultrasonic.h
 * Phase 2 - HC-SR04 on D4 / PB5 and D6 / PB10
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */
#ifndef NUCLEO476_ULTRASONIC_H_
#define NUCLEO476_ULTRASONIC_H_
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h> // For null pointer

// #defines
#define ULTRASONIC_INTERVAL_US   (80000U)
#define ULTRASONIC_TIMEOUT_US    (30000U)
#define ULTRASONIC_TRIGGER_US	 (10U)
#define TIM2_INTERRUPT_PRIORITY  (1U)
// Distance conversion: 343 m/s = 0.343 mm/us, and the echo is a round trip,
// so distance_mm = echo_us * 0.343 / 2 = echo_us * 343 / 2000.
#define SOUND_MM_PER_US_NUM (343)
#define SOUND_MM_PER_US_DEN (2000)


// Enums
typedef enum
{
	ULTRASONIC_IDLE_STATE,			// Before TRIG pulse
	ULTRASONIC_TRIGGER_STATE, 		// During TRIG pulse
	ULTRASONIC_BURST_STATE,			// Sensor is sending sonic burst
	ULTRASONIC_ECHO_STATE,			// Sensor is waiting for echo
	ULTRASONIC_COMPLETE_STATE,		// Sensor has received echo
	ULTRASONIC_TIMEOUT_STATE		// Sensor never recieved echo
} Ultrasonic_State;

typedef enum
{
	ULTRASONIC_OK,					// Distance has been calculated
	ULTRASONIC_TIMEOUT,				// Timed out while waiting for echo
	ULTRASONIC_ERROR,				// An error occurred finding a distance
	ULTRASONIC_NOT_READY			// Sensor has not found a distance
} Ultrasonic_Result;

typedef struct
{
	uint32_t sequence;				// ??? TODO - what is this
	uint32_t timestamp_us;			// Current time
	uint32_t echo_us;				// Pulse width
	uint32_t distance_mm;			// Calculated distance
	Ultrasonic_Result result;		// Result
} Ultrasonic_Sample;


// Call after timer_init and keep its free-running TIM2 timebase intact
void Ultrasonic_Init(void);

// Return false when a new measurement cannot be started
bool Ultrasonic_Start(uint32_t now_us);

// Keep measurements moving and handle a missing echo without waiting
void Ultrasonic_Service(uint32_t now_us);

// Accessor for the current state
Ultrasonic_State GetState(void);

// Determine if the result is valid and the current state is "COMPLETE."
bool Ultrasonic_HasNewSample(void);

// Return true only when you copy a new complete result into sample
// Return false for a null pointer or when there is no result to take
bool Ultrasonic_TakeSample(Ultrasonic_Sample *sample);

// Interrupt handler for TIM2. Used for TIM2_CH3 capture/compare.
void TIM2_IRQHandler(void);


// NOTES:
/**** Ultrasonic_Start ****/
// If not idle, reject and return false
	// Raise TRIG and record time
	// set state trig high
	// Return true
/**************************/

/**** Ultrasonic_Service ****/
// If state is TRIGGER_HIGH and at least 10 us elapsed
		// Set TRIG LOW
		// Arm/confirm rising-edge capture
		// Save measurement_start_us
		// Set state = WAIT_ECHO
	// If state is WAIT_ECHO and Echo completed
		// Convert pulse width to distance
		// Publish the new sample
		// Set state = COMPLETE
	// If state is WAIT_ECHO and timeout elapsed
		// Mark timeout
		// Restore capture for the next request
		// Set state = TIMEOUT
/****************************/

/**** TIM2_IRQHandler ****/
// If CC3IF is set
		// Read CCR3
		// Clear the channel 3 capture flag as required
		// If waiting for rising edge
			// Save t_rise
			// Configure the next capture for falling edge
			// Set capture state to WAIT_FALL
		// Else if waiting for falling edge
			// Save t_fall
			// Mark the sample complete
			// Restore rising-edge capture for the next measurement
/*************************/

/**** Wavedrom State and Timing Diagram ****/
/*
{ "signal": [
  { "name": "Trigger", "wave": "L....H...L........................" },
  { "name": "Sonic Burst",   "wave": "L..........HLHLHLHL..............." },
  { "name": "Echo",       "wave": "L...................H........L...." },
  {name: 'current_state', wave: '6....3...4..........5........7....', data: ['IDLE', 'TRIGGER', 'BURST', 'ECHO', 'COMPLETE']},
  {name: 'Timeout', wave: '........x8...................x....', data: ['trigger_time_elapsed']},
  {name: 'pulse_width', wave: '...................x8........x....', data: ['pulse_width']},
],
  "head": {
    "text": "Ultrasonic State and Timing Diagram"
  }
}
*/
/*******************************************/


#endif /* NUCLEO476_ULTRASONIC_H_ */
