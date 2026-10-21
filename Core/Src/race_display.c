/*
 * race_display.c
 * Display for Project 3
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */


#include "race_display.h"
#include "project_config.h"
#include "uart.h"
#include <stdio.h>
#include <stddef.h>

// This module formats the display supplied by race.c or phase_tests.c
// Fill the display with application data before calling the display service
// Keep driving decisions and hardware control in the application modules

// Refresh the dashboard at most four times per second
// If the UART queue is busy, leave 50 ms before trying again
#define DISPLAY_MS             250U
#define DISPLAY_RETRY_MS       50U

// Use the same distance units as the car model
// These are absolute positions along the complete route
#define TRACK_MM               (PROJECT_TRACK_MILES * PROJECT_MILE_MM)
#define FIRST_CORNER_MM        (PROJECT_FIRST_CORNER_MILES * PROJECT_MILE_MM)
#define SECOND_CORNER_MM       (PROJECT_SECOND_CORNER_MILES * PROJECT_MILE_MM)

// Each entry is the distance at the end of one section
static const uint32_t s_section_end_mm[] = {FIRST_CORNER_MM, SECOND_CORNER_MM, TRACK_MM};

// Keep the last successful update separate from the last queue attempt
static uint32_t s_display_ms = 0;
static uint32_t s_attempt_ms = 0;

// Remember the layout and car marker accepted into the UART queue
// These values describe display work, not the car's driving state
static bool s_layout_ready = false;
static bool s_car_drawn = false;
static uint8_t s_car_row = 0;
static uint8_t s_car_col = 0;
static char s_car_glyph = 0;

//******************************************************************************************
// Function: RaceDisplay_Init
//
// Purpose:
//      Prepare the display before the foreground starts servicing it
//
// Inputs:
//      None
//
// Output:
//      None
//******************************************************************************************
void RaceDisplay_Init(void)
{
	// Unsigned subtraction makes the first update eligible even at time zero
	s_display_ms = 0U - DISPLAY_MS;
	s_attempt_ms = 0U - DISPLAY_RETRY_MS;

	// The next service call needs to queue the layout and then the car marker
	s_layout_ready = false;
	s_car_drawn = false;
	s_car_row = 0;
	s_car_col = 0;
	s_car_glyph = 0;
}

//******************************************************************************************
// Function: Display_GetMarker
//
// Purpose:
//      Map the car's route position to a row, column and direction symbol
//
// Inputs:
//      view - snapshot supplied by the application
//      row, col, glyph - addresses where the marker values will be stored
//
// Output:
//      True when the section and position agree and marker values are ready
//      False when the snapshot places the car outside its selected section
//******************************************************************************************
static bool Display_GetMarker(const RaceDisplay_Snapshot *view, uint8_t *row, uint8_t *col, char *glyph)
{
	// Check the section before using it as an array index
	if (view->section > 2U || view->position_mm > TRACK_MM)
		return false;

	// Section 0 starts at zero and later sections start at the previous corner
	uint32_t start = view->section == 0U ? 0U : s_section_end_mm[view->section - 1U];

	// Let the dashboard show CHECK VIEW if the application data disagrees
	if (view->position_mm < start || view->position_mm > s_section_end_mm[view->section])
		return false;

	// Coordinates here are relative to the road, starting at row 0 and column 0
	// Each section maps its distance onto the cells assigned to it
	// Use a wide intermediate value when multiplying the distance
	*glyph = '>';
	if (view->section == 0U)
	{
		// First straight: mile 0 to 40, across columns 0 to 20 on the top row
		*row = 0;
		*col = (uint8_t)((uint64_t)view->position_mm * 20U / FIRST_CORNER_MM);
	}
	else if (view->section == 1U)
	{
		// Vertical section: mile 40 to 60, down rows 0 to 5 at column 20
		*row = (uint8_t)((uint64_t)(view->position_mm - FIRST_CORNER_MM) * 5U / (SECOND_CORNER_MM - FIRST_CORNER_MM));
		*col = 20;
		*glyph = 'v';
	}
	else
	{
		// Last straight: mile 60 to 100, across columns 20 to 40 on the bottom row
		*row = 5;
		*col = (uint8_t)(20U + (uint64_t)(view->position_mm - SECOND_CORNER_MM) * 20U / (TRACK_MM - SECOND_CORNER_MM));
	}
	return true;
}

//******************************************************************************************
// Function: Display_RoadAt
//
// Purpose:
//      Find the road character to put back when the car leaves a cell
//
// Inputs:
//      row, col - coordinates relative to the road
//
// Output:
//      The original road character for that cell
//******************************************************************************************
static char Display_RoadAt(uint8_t row, uint8_t col)
{
	// Both corners use a plus sign
	if ((row == 0U || row == 5U) && col == 20U)
		return '+';

	// The straight sections use a dash and the vertical section uses a bar
	return (row == 0U || row == 5U) ? '-' : '|';
}

//******************************************************************************************
// Function: RaceDisplay_Service
//
// Purpose:
//      Keep the road on screen and queue updates for the car and dashboard
//
// Inputs:
//      view - consistent snapshot of the current application data
//      now_ms - real elapsed milliseconds from the timebase
//
// Output:
//      None
//******************************************************************************************
void RaceDisplay_Service(const RaceDisplay_Snapshot *view, uint32_t now_ms)
{
	// Return when no snapshot is available or the next update is not due
	// Unsigned elapsed time still works when the millisecond counter wraps
	if (!view || (uint32_t)(now_ms - s_display_ms) < DISPLAY_MS || (uint32_t)(now_ms - s_attempt_ms) < DISPLAY_RETRY_MS)
		return;

	// Record this attempt even if the UART queue cannot accept the message
	s_attempt_ms = now_ms;

	if (!s_layout_ready)
	{
		// Draw the title, road, labels and controls once after initialization
		// The octal escape \033 represents ESC in these strings
		// ESC[2J clears the screen, ESC[H moves home and ESC[?25l hides the cursor
		// Later updates leave this layout in place so the screen stays steady
		static const char layout[] = "\033[2J\033[H\033[?25lProject 3 - 100-mile Race\r\nRoute: 40 + 20 + 40 miles | Clock: 100x | Map: schematic\r\n\r\nSTART --------------------+ RIGHT (mile 40)\r\n                          |\r\n                          | 20 miles\r\n                          |\r\n                          |\r\n                          +-------------------- FINISH (mile 100)\033[10;27HLEFT (mile 60)\033[19;1HHold Up/W: drive  Down/S: brake  Left/Right/A/D: turn\033[20;1HR: restart  B1: stop / resume after phase 4";

		// The queue copies the layout without its terminating null character
		// Mark it ready only when the whole message has been accepted
		if (USART_TryWrite(layout, sizeof(layout) - 1U))
			s_layout_ready = true;

		// Return to the main loop and add the changing fields on a later call
		return;
	}

	// Check the snapshot and work out where its marker belongs
	uint8_t row = 0;
	uint8_t col = 0;
	char glyph = '>';
	bool valid = Display_GetMarker(view, &row, &col, &glyph);

	// This small string holds commands to restore the old cell and draw the new one
	// If the marker has not changed, it stays empty and only the dashboard updates
	char marker[64] = "";

	// Restore the old road cell when the marker moves, changes direction or is invalid
	// ANSI terminal coordinates start at 1 and our road starts at row 4, column 7
	if (s_car_drawn && (!valid || row != s_car_row || col != s_car_col || glyph != s_car_glyph))
		(void)snprintf(marker, sizeof(marker), "\033[%lu;%luH%c", (unsigned long)(s_car_row + 4U), (unsigned long)(s_car_col + 7U), Display_RoadAt(s_car_row, s_car_col));

	if (valid && (!s_car_drawn || row != s_car_row || col != s_car_col || glyph != s_car_glyph))
	{
		// Append the new marker after any command that restores the old cell
		size_t used = 0;
		while (marker[used] != '\0')
			used++;
		(void)snprintf(marker + used, sizeof(marker) - used, "\033[%lu;%luH%c", (unsigned long)(row + 4U), (unsigned long)(col + 7U), glyph);
	}

	// Keep fractional display values as integers
	// progress holds hundredths of a mile and speed holds tenths of a mile per hour
	// The wide products avoid overflow before division and the speed is rounded
	uint32_t progress = (uint32_t)((uint64_t)view->position_mm * 100U / PROJECT_MILE_MM);
	uint32_t speed = (uint32_t)(((uint64_t)view->velocity_mm_s * 36000U + PROJECT_MILE_MM / 2U) / PROJECT_MILE_MM);

	// Driving time comes from the virtual car clock, not the real refresh clock
	uint32_t seconds = view->drive_time_ms / 1000U;

	// Keep a useful message if the section and position cannot be mapped
	char next[40] = "CHECK POSITION";
	if (valid)
	{
		// Find the distance to the next corner or finish
		uint32_t remaining_mm = s_section_end_mm[view->section] - view->position_mm;
		// Round up so the display does not say 0.00 miles before we reach the endpoint
		uint32_t remaining = (uint32_t)(((uint64_t)remaining_mm * 100U + PROJECT_MILE_MM - 1U) / PROJECT_MILE_MM);
		const char *direction = (view->section == 0U) ? "RIGHT" : ((view->section == 1U) ? "LEFT" : "FINISH");
		// At the endpoint, show the action instead of another distance
		if (remaining_mm == 0)
			(void)snprintf(next, sizeof(next), "%s %s", view->section < 2U ? "TURN" : "AT", direction);
		else
			(void)snprintf(next, sizeof(next), "%s in %lu.%02lu miles", direction, (unsigned long)(remaining / 100U), (unsigned long)(remaining % 100U));
	}

	// Leave distance fields as dashes until a fresh valid reading is available
	// The application supplies freshness, result status and the blocked flag
	char physical[20] = "--";
	char simulated[20] = "--";
	const char *obstacle = !view->sensor_fresh ? "NO FRESH SAMPLE" : (view->sensor_result == ULTRASONIC_OK ? (view->obstacle ? "BLOCKED" : "CLEAR") : (view->sensor_result == ULTRASONIC_TIMEOUT ? "NO ECHO" : "SENSOR ERROR"));
	if (view->sensor_fresh && view->sensor_result == ULTRASONIC_OK)
	{
		// distance holds tenths of a simulated meter after applying the display scale
		// At scale 100, a physical 200 mm reading is 20.0 cm and 20.0 simulated m
		// The car still uses the physical millimeter reading for its braking decision
		uint32_t distance = (uint32_t)((uint64_t)view->sensor_distance_mm * PROJECT_SENSOR_SCALE / 100U);
		(void)snprintf(physical, sizeof(physical), "%lu.%lu cm", (unsigned long)(view->sensor_distance_mm / 10U), (unsigned long)(view->sensor_distance_mm % 10U));
		(void)snprintf(simulated, sizeof(simulated), "%lu.%lu m", (unsigned long)(distance / 10U), (unsigned long)(distance % 10U));
	}

	// Show a position only when the application has accepted a servo command
	char servo[16] = "--";
	if (view->servo_position_valid)
		(void)snprintf(servo, sizeof(servo), "%lu/5", (unsigned long)view->servo_position);

	// Build one complete update before offering it to the UART queue
	// ESC[row;colH moves the cursor and ESC[K clears the rest of that line
	// Clearing the line removes leftover characters when the new text is shorter
	// The text limits keep application labels from running across the screen
	// Missing labels use dashes and an invalid marker adds CHECK VIEW
	// Format driving time as hours, minutes and seconds, and uptime from real time
	char message[512];
	int length = snprintf(message, sizeof(message), "%s\033[11;1HState: %.24s%s\033[K\033[12;1HSpeed: %lu.%lu mph  Progress: %lu.%02lu / 100 miles\033[K\033[13;1HNext: %s  Drive time: %lu:%02lu:%02lu\033[K\033[14;1HObstacle: %s  Sim: %s  Sensor: %s\033[K\033[15;1HServo: %.20s %s\033[K\033[16;1HLast key: %.12s at %lu ms  Uptime: %lu s\033[K\033[17;1HDevelopment mode: %lu\033[K", marker, view->state_text ? view->state_text : "--", valid ? "" : " [CHECK VIEW]", (unsigned long)(speed / 10U), (unsigned long)(speed % 10U), (unsigned long)(progress / 100U), (unsigned long)(progress % 100U), next, (unsigned long)(seconds / 3600U), (unsigned long)(seconds % 3600U / 60U), (unsigned long)(seconds % 60U), obstacle, simulated, physical, view->servo_text ? view->servo_text : "--", servo, view->last_key_text ? view->last_key_text : "--", (unsigned long)view->last_key_ms, (unsigned long)(now_ms / 1000U), (unsigned long)PROJECT_PHASE);

	// snprintf reports the length it needed, even when the buffer was too small
	// Queue only a complete message and leave other services running if it cannot fit
	if (length > 0 && (size_t)length < sizeof(message) && USART_TryWrite(message, (size_t)length))
	{
		// Remember what was accepted so the next update restores the correct road cell
		// A busy queue leaves these values unchanged and the next call can retry
		s_display_ms = now_ms;
		s_car_drawn = valid;
		if (valid)
		{
			s_car_row = row;
			s_car_col = col;
			s_car_glyph = glyph;
		}
	}
}
