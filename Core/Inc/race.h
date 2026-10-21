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

void Race_Init(void);
void Race_Service(uint32_t now_ms);
void Race_GetSnapshot(RaceDisplay_Snapshot *view);

// gpio.c calls this from the B1 interrupt
void B1_EXTI_Callback(uint16_t pin);

#endif /* NUCLEO476_RACE_H_ */
