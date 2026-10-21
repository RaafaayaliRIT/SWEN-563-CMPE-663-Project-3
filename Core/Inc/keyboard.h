/*
 * keyboard.h
 * keyboard input to control race for Project 3
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */
#ifndef KEYBOARD_H_
#define KEYBOARD_H_
#include <stdint.h>
#include <stdbool.h>

typedef enum {
	KEY_UP,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_RESTART,
	KEY_INPUT_ERROR
} Keyboard_Command;

typedef struct {
	Keyboard_Command command;
	uint32_t timestamp_ms; // When the command's last byte arrived
} Keyboard_Event;

// Register the UART callbacks before USART2_EnableIRQ
void Keyboard_Init(void);

// Decode a bounded number of received bytes in the foreground
void Keyboard_Service(uint32_t now_ms);

// Return one command and its receive time, or false when none is available
bool Keyboard_TakeEvent(Keyboard_Event *event);

// Drop old commands and wait for a short quiet interval before accepting more
// Decide when your application needs this, such as stop or resume
void Keyboard_DiscardPending(uint32_t now_ms);

const char *Keyboard_CommandName(Keyboard_Command command);

#endif /* KEYBOARD_H_ */
