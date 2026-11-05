#include "custom_functions.h"


// true when time A is earlier than time B
bool Time_Before(uint32_t a, uint32_t b)
{
	return (int32_t)(a - b) < 0;
}

uint64_t CalculateTimeElapsed(uint32_t start_us, uint32_t end_us)
{
	// Create variable to hold final time elapsed
	uint64_t time_elapsed;

	// If there was not counter reset
	if(end_us >= start_us)
	{
		time_elapsed = end_us - start_us;
	}
	// If there was a counter reset
	else
	{
		time_elapsed = (end_us + TIM_CNT_CNT) - start_us;
	}

	// Return final time elapsed
	return time_elapsed;
}
