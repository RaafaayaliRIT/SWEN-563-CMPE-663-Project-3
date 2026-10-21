/*
 * phase_tests.h
 * Optional development exercises for the servo and sensor
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */
#ifndef PHASE_TESTS_H_
#define PHASE_TESTS_H_
#include <stdint.h>
#include "race_display.h"

void PhaseTests_Init(void);
void PhaseTests_Service(uint32_t now_ms);
void PhaseTests_GetSnapshot(RaceDisplay_Snapshot *view);

#endif /* PHASE_TESTS_H_ */
