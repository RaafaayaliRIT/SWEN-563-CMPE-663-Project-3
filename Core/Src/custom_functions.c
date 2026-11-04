#include "custom_functions.h"


uint64_t CalculateTimeElapsed(uint32_t now_us, uint32_t previous_us)
{
	// Create variable to hold final time elapsed
	uint64_t time_elapsed;

	// If there was not counter reset
	if(now_us >= previous_us)
	{
		time_elapsed = now_us - previous_us;
	}
	// If there was a counter reset
	else
	{
		time_elapsed = (now_us + TIM_CNT_CNT) - previous_us;
	}

	// Return final time elapsed
	return time_elapsed;
}
