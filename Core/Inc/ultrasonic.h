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

#define ULTRASONIC_INTERVAL_US   80000U
#define ULTRASONIC_TIMEOUT_US    30000U

// These are measurement results, not the sensor's internal states
// Choose your own internal states as you design the measurement sequence
typedef enum {
	ULTRASONIC_OK,
	ULTRASONIC_TIMEOUT,
	ULTRASONIC_ERROR,
	ULTRASONIC_NOT_READY
} Ultrasonic_Result;

typedef struct {
	uint32_t sequence;
	uint32_t timestamp_us;
	uint32_t echo_us;
	uint32_t distance_mm;
	Ultrasonic_Result result;
} Ultrasonic_Sample;

// Call after timer_init and keep its free-running TIM2 timebase intact
void Ultrasonic_Init(void);

// Return false when a new measurement cannot be started
bool Ultrasonic_Start(uint32_t now_us);

// Keep measurements moving and handle a missing echo without waiting
void Ultrasonic_Service(uint32_t now_us);

// Return true only when you copy a new complete result into sample
// Return false for a null pointer or when there is no result to take
bool Ultrasonic_TakeSample(Ultrasonic_Sample *sample);

void TIM2_IRQHandler(void);

#endif /* NUCLEO476_ULTRASONIC_H_ */
