/*
 * race_display.h
 * Display for Project 3
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 *
 */
#ifndef RACE_DISPLAY_H_
#define RACE_DISPLAY_H_
#include <stdint.h>
#include <stdbool.h>
#include "ultrasonic.h"

// This is a screen snapshot, not your car state machine
// Keep your application data in the structure that makes sense for your design
// Fill these fields when you want to show that data on the terminal
// Text pointers should refer to strings that remain available after the call
typedef struct {
	const char *state_text;
	uint32_t velocity_mm_s;
	uint32_t position_mm;
	uint32_t drive_time_ms;
	uint8_t section; 			// 0 = first straight, 1 = vertical, 2 = last straight
	const char *servo_text;
	uint8_t servo_position;
	bool servo_position_valid;
	bool sensor_fresh;
	Ultrasonic_Result sensor_result;
	uint32_t sensor_distance_mm;
	bool obstacle;
	const char *last_key_text;
	uint32_t last_key_ms;
} RaceDisplay_Snapshot;

void RaceDisplay_Init(void);

// Call from the foreground with a consistent snapshot
// The renderer handles its own refresh interval and UART queue retry
void RaceDisplay_Service(const RaceDisplay_Snapshot *view, uint32_t now_ms);

#endif /* RACE_DISPLAY_H_ */
