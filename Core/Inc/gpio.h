/*
 * gpio.h
 *
 *  Created on: Nov 4, 2021
 *      Author: Mitesh Parikh
 *
 *      Updated on: Jan 21, 2026
 *      Author: Mitesh Parikh
 *      Comments: Added more comments, included header files
 */



//This is Bare-metal GPIO/EXTI init for NUCLEO-L476RG user button (B1 on PC13).
//
//Key idea here is to:
//	- Configure PC13 as input
//	- Route EXTI line 13 to Port C via SYSCFG
//	- Trigger interrupt on falling edge (button press)
//	- Enable EXTI15_10_IRQn in NVIC

#ifndef INC_GPIO_H_
#define INC_GPIO_H_

#include <stdint.h>

// Initialize GPIO and EXTI for the user push-button (B1 on PC13)
void GPIO_Init(void);

#endif /* INC_GPIO_H_ */
