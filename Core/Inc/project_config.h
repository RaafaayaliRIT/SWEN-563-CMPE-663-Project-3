/*
 * project_config.h
 * Project 3 starter
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 *
 */
#ifndef PROJECT_CONFIG_H_
#define PROJECT_CONFIG_H_

// Use this to try modules on their own or run your integrated application
// 1 = servo, 2 = sensor, 3 = driving, 4 = B1, 5 = LED
// The phases suggest a development order and there is one final demo
// Use 5 for your finished solution
#ifndef PROJECT_PHASE
#define PROJECT_PHASE                 1U
#endif

#if PROJECT_PHASE < 1 || PROJECT_PHASE > 5
#error Select a project phase from 1 through 5
#endif

// These values describe the assignment
// Work out your timer settings and unit conversions from them
#define PROJECT_CLOCK_HZ              	80000000U
#define PROJECT_MILE_MM               	1609344U
#define PROJECT_TRACK_MILES           	100U
#define PROJECT_FIRST_CORNER_MILES    	40U
#define PROJECT_SECOND_CORNER_MILES   	60U
#define PROJECT_TIME_SCALE            	100U
#define PROJECT_STEP_MS               	10U
#define PROJECT_MAX_SPEED_MPH         	100U
#define PROJECT_SPEED_STEP_MPH        	5U
#define PROJECT_SPEED_KEY_MS         	100U
#define PROJECT_FIRST_REPEAT_MS      	750U
#define PROJECT_UP_TIMEOUT_MS        	200U
#define PROJECT_SERVO_STEP_MS        	250U
#define PROJECT_SENSOR_SCALE         	100U
#define PROJECT_OBSTACLE_MM          	200U
#define PROJECT_SENSOR_STALE_US      	250000U
#define PROJECT_BUTTON_RELEASE_MS    	50U
#define PROJECT_INPUT_BUDGET         	16U

#endif /* PROJECT_CONFIG_H_ */
