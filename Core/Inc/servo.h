/*
 * servo.h
 * Phase 1 - one servo to be used only, on D5 / PB4
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */
#ifndef NUCLEO476_SERVO_H_
#define NUCLEO476_SERVO_H_
#include <stdint.h>
#include <stdbool.h>

// Use the agreed pulse range and check it with your servo
#define SERVO_MIN_US             1000U
#define SERVO_MAX_US             2000U
#define SERVO_LAST_POSITION      5U

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

// Set up PB4 / TIM3_CH1 for 50 Hz PWM and start at position 0
void Servo_Init(void);

// Return false for an invalid pulse and leave the previous output alone
bool Servo_SetPulseUs(uint32_t pulse_us);

// Positions 0 through 5 cover the agreed pulse range
bool Servo_SetPosition(uint8_t position);

#endif /* NUCLEO476_SERVO_H_ */
