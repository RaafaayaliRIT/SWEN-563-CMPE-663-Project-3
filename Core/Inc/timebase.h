/*
 * timebase.h
 * Supplied time source for Project 3 - Systick
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */
#ifndef TIMEBASE_H_
#define TIMEBASE_H_
#include <stdint.h>

// Start the same 1 ms SysTick time source used in the previous project
void init_systick(void);
uint32_t timebase_get_ms(void);

#endif /* TIMEBASE_H_ */
