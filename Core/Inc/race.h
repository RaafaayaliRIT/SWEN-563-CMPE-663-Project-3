/*
 * race.h
 * Application for phases 3 through 5
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 *
 */
#ifndef NUCLEO476_RACE_H_
#define NUCLEO476_RACE_H_
#include <stdint.h>
#include "race_display.h"

// Race #defines
#define RACE_STALL_MS (50) 				// driving service late by more than this -> stop
#define RACE_MS_PER_HOUR (3600000) 		// for mph -> mm per virtual ms
#define RACE_WARNING_MM (400)			// warning range: above 200mm and at or below 400mm, keep driving

// LED #defines
#define RACE_LED_1HZ_HALF_MS (500) 		// obstacle brake: toggle every 500ms
#define RACE_LED_2HZ_HALF_MS (250)		// B1 stop or fault: toggle every 250ms

// Servo #defines
#define RACE_STEER_RIGHT_POS (5)		// servo position shown after a Right turn
#define RACE_STEER_LEFT_POS (0)			// servo position shown after a Left turn

// Enums
typedef enum {
	CAR_READY,							// on the route, stopped, waiting for a fresh Up
	CAR_DRIVING,						// speed > 0, position advancing
	CAR_CORNER,							// stopped exactly at mile 40 or 60, waiting for the correct turn
	CAR_FINISHED,						// reached mile 100; only R starts a new race
	CAR_FAULT							// input error or stalled service; only R recovers
} Car_State;

// Servo animation chosen from the car condition
typedef enum {
	SERVO_MODE_HOLD,					// ordinary stop or fault: keep the current position
	SERVO_MODE_SWEEP,					// moving: 0,1,2,3,4,5,4,3,2,1,0 repeating
	SERVO_MODE_STUCK,					// obstacle or corner: alternate previous and current
	SERVO_MODE_FINALE,					// finished: up to 5, then down to 0, once
	SERVO_MODE_DONE						// finale complete: stay at 0
} Servo_Mode;

// LD2 output chosen from the car condition
typedef enum {
	LED_PATTERN_OFF,					// ready, ordinary stop, corner wait, finished
	LED_PATTERN_ON,						// moving
	LED_PATTERN_BLINK_1HZ,				// automatic obstacle brake
	LED_PATTERN_BLINK_2HZ 				// B1 emergency stop or fault
} Led_Pattern;

// Race prototypes
void Race_Init(void);
void Race_Service(uint32_t now_ms);
void Race_GetSnapshot(RaceDisplay_Snapshot *view);

// gpio.c calls this from the B1 interrupt
void B1_EXTI_Callback(uint16_t pin);

#endif /* NUCLEO476_RACE_H_ */
