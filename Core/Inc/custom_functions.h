#ifndef INC_CUSTOM_FUNCTIONS_H_
#define INC_CUSTOM_FUNCTIONS_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32l476xx.h"

// Used to calculate the elapsed time when using a 32 bit timer counter value
// that may have rolled over once. Multiple rollovers are not supported.
uint64_t CalculateTimeElapsed(uint32_t start_us, uint32_t end_us);

// true when time A is earlier than time B
bool Time_Before(uint32_t a, uint32_t b);

#endif /* INC_CUSTOM_FUNCTIONS_H_ */
