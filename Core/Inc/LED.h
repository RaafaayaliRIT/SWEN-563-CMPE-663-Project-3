/*
 * led.h
 *
 *  Created on: Nov 4, 2021
 *      Author: Mitesh Parikh
 *
 *      Update on: Jan 21, 2025
 *      Author: Mitesh Parikh
 *      Comments: Revised and removed unnecessary functions, simplified bare-metal API
 */

#ifndef NUCLEO476_LED_H_
#define NUCLEO476_LED_H_

#include <stdbool.h>

//
// NUCLEO-L476RG User LED (LD2) is connected to PA5
// This module provides bare-metal control of LD2
//

// Initialize GPIO for the on-board user LED (LD2 on PA5)
void LED_Init(void);

// Turn the LED ON
void LED_On(void);

// Turn the LED OFF
void LED_Off(void);

// Toggle the current state of the LED
void LED_Toggle(void);

// Return true if the LED is ON, false otherwise
bool LED_IsOn(void);


#endif /* NUCLEO476_LED_H_ */

/* __NUCLEO476_LED_H */
