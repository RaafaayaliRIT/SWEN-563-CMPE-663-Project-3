/*
 * timer.h
 *
 *  Created on: Feb 2, 2026
 *      Author: Mitesh Parikh
 *
 */
 
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

// TIM2 Microsecond Timer
// Initialize TIM2 as a 1 MHz free-running timer
// System clock is fixed at 80 MHz for this course
// Timer is free-running and wraps naturally at 2^32
void timer_init(void);

// Start - stop - reset helper functions
void timer_start(void);
void timer_stop(void);
void timer_reset(void);

// Get current timestamp in microseconds
uint32_t timer_get_us(void);

// Compute elapsed time in microseconds
uint32_t timer_elapsed_us(uint32_t start_us, uint32_t now_us);

#endif /* TIMER_H */
