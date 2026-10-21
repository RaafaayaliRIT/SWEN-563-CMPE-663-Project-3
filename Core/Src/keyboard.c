/*
 * keyboard.c
 * keyboard input to control race for Project 3
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */

#include "keyboard.h"
#include "uart.h"
#include "timebase.h"
#include "stm32l476xx.h"

// The ISR saves bytes and the foreground turns them into commands
// race.c decides what those commands mean for the car
// Command times let the application detect when Up repeats have stopped

// Two queues keep byte reception separate from application command handling
// Each queue leaves one slot unused to distinguish full from empty
// Limit decoding work per call so the other services can keep running
#define KEYBOARD_RX_SIZE        128U
#define KEYBOARD_EVENT_SIZE     32U
#define KEYBOARD_RX_BUDGET      16U

// Allow a short time for the remaining bytes of an arrow sequence
// After lost or damaged input, wait for a quiet interval before accepting more
#define KEYBOARD_ESCAPE_MS      50U
#define KEYBOARD_QUIET_MS       50U

// These are keyboard decoder states, separate from the car's states
// Remember the prefix because an arrow can arrive across several UART interrupts
typedef enum {
	PARSE_NORMAL, // Ready for an ordinary letter or a new escape sequence
	PARSE_ESCAPE, // Saw ESC and need the next prefix byte
	PARSE_CSI, // Saw ESC [ and need the arrow direction
	PARSE_SS3, // Saw ESC O and need the arrow direction
	PARSE_DISCARD, // Consume an unsupported or expired sequence through its final byte
	PARSE_DISCARD_ESCAPE // Consume a late prefix before discarding its remaining bytes
} Parse_State;

// Save the receive time with each byte
// Processing it later must not make an old key press look like a new one
typedef struct {
	char value;
	uint32_t timestamp_ms;
} Rx_Byte;

// The USART2 ISR adds records at head and the foreground takes them from tail
// Shared indices and flags are volatile so the compiler reads their current values
// Brief interrupt protection below keeps related values consistent
static Rx_Byte s_rx[KEYBOARD_RX_SIZE];
static volatile uint32_t s_rx_head = 0;
static volatile uint32_t s_rx_tail = 0;

// Update activity even while discarding bytes so the quiet interval restarts
// A fault means input was lost or damaged, and resync pauses acceptance of new bytes
static volatile uint32_t s_activity_ms = 0;
static volatile bool s_rx_fault = false;
static volatile bool s_resync = false;

// Only foreground code writes and reads the decoded event queue
static Keyboard_Event s_events[KEYBOARD_EVENT_SIZE];
static uint32_t s_event_head = 0;
static uint32_t s_event_tail = 0;

// Keep an error event separate so a full command queue cannot hide it
static bool s_error_pending = false;
static uint32_t s_error_ms = 0;

// Save the current sequence state and the time its ESC byte arrived
static Parse_State s_parse = PARSE_NORMAL;
static uint32_t s_escape_ms = 0;

//******************************************************************************************
// Function: Keyboard_RxCallback
//
// Purpose:
//      Save a received byte and its time without decoding it in the ISR
//
// Inputs:
//      c - byte passed by the USART2 interrupt handler
//
// Output:
//      None
//******************************************************************************************
static void Keyboard_RxCallback(char c)
{
	// Record when the callback runs, even if this byte will be discarded
	s_activity_ms = timebase_get_ms();

	// Leave recovery to the foreground and keep this ISR callback short
	if (s_rx_fault || s_resync)
		return;

	// Wrap to the start of the buffer after its last slot
	uint32_t next = (s_rx_head + 1U) % KEYBOARD_RX_SIZE;

	// Catching the tail means the queue is full, so report loss instead of overwriting
	if (next == s_rx_tail)
	{
		s_rx_fault = true;
		return;
	}

	// Store the byte and its time before publishing the new head
	s_rx[s_rx_head].value = c;
	s_rx[s_rx_head].timestamp_ms = s_activity_ms;

	// Make both fields visible before the foreground can see this record
	__DMB();
	s_rx_head = next;
}

//******************************************************************************************
// Function: Keyboard_RxErrorCallback
//
// Purpose:
//      Record a UART input error for the foreground to handle
//
// Inputs:
//      errors - UART error bits reported by the driver
//
// Output:
//      None
//******************************************************************************************
static void Keyboard_RxErrorCallback(uint32_t errors)
{
	// All reported UART errors use the same recovery path here
	// Record the problem and let foreground code clear the queues and parser
	(void)errors;
	s_activity_ms = timebase_get_ms();
	s_rx_fault = true;
}

//******************************************************************************************
// Function: Keyboard_Init
//
// Purpose:
//      Prepare the queues and register the UART callbacks before enabling IRQ mode
//
// Inputs:
//      None
//
// Output:
//      None
//******************************************************************************************
void Keyboard_Init(void)
{
	// Clear queues, flags and partial-sequence state before callbacks can run
	s_rx_head = 0;
	s_rx_tail = 0;
	s_event_head = 0;
	s_event_tail = 0;
	s_activity_ms = 0;
	s_rx_fault = false;
	s_resync = false;
	s_error_pending = false;
	s_error_ms = 0;
	s_parse = PARSE_NORMAL;
	s_escape_ms = 0;

	// Connect byte and error reception to these short ISR callbacks
	USART2_RegisterRxCallback(Keyboard_RxCallback);
	USART2_RegisterRxErrorCallback(Keyboard_RxErrorCallback);
}

//******************************************************************************************
// Function: Keyboard_DiscardPending
//
// Purpose:
//      Drop old input from the foreground and start a quiet interval
//
// Inputs:
//      now_ms - caller time, a fresh timebase reading starts the quiet interval
//
// Output:
//      None
//******************************************************************************************
void Keyboard_DiscardPending(uint32_t now_ms)
{
	// This function returns right away and Keyboard_Service checks the quiet interval
	// Use a fresh clock reading because the caller may have captured its time earlier
	(void)now_ms;

	// Save the existing IRQ mask and protect changes to ISR-shared data
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	// Moving tail to head empties the byte queue and starts the quiet interval
	s_rx_tail = s_rx_head;
	s_resync = true;
	s_activity_ms = timebase_get_ms();

	// Restore the previous IRQ mask rather than always enabling interrupts
	__set_PRIMASK(primask);

	// Drop decoded commands and any unfinished arrow together
	// Leave pending error flags intact so discarding commands cannot hide a fault
	s_event_tail = s_event_head;
	s_parse = PARSE_NORMAL;
}

//******************************************************************************************
// Function: Keyboard_Publish
//
// Purpose:
//      Put one decoded command and its receive time in the event queue
//
// Inputs:
//      command - decoded keyboard command
//      stamp_ms - time its final byte was received
//
// Output:
//      None
//******************************************************************************************
static void Keyboard_Publish(Keyboard_Command command, uint32_t stamp_ms)
{
	// Use the same wrap and unused-slot rule as the raw byte queue
	uint32_t next = (s_event_head + 1U) % KEYBOARD_EVENT_SIZE;
	if (next == s_event_tail)
	{
		// A lost command makes the queued input unreliable
		// Remember the error and discard old input before starting recovery
		s_error_pending = true;
		s_error_ms = stamp_ms;
		Keyboard_DiscardPending(stamp_ms);
		return;
	}

	// Keep the receive time, even if decoding happens much later
	s_events[s_event_head].command = command;
	s_events[s_event_head].timestamp_ms = stamp_ms;
	s_event_head = next;
}

//******************************************************************************************
// Function: Keyboard_Letter
//
// Purpose:
//      Translate a supported letter into a keyboard command
//
// Inputs:
//      c - ordinary character received outside an escape sequence
//      stamp_ms - receive time for this character
//
// Output:
//      None
//******************************************************************************************
static void Keyboard_Letter(char c, uint32_t stamp_ms)
{
	// Accept uppercase and lowercase letters with one set of mappings
	if (c >= 'A' && c <= 'Z')
		c = (char)(c + ('a' - 'A'));

	// W, S, A and D match the arrows, and R requests a restart
	// Other ordinary characters are ignored
	if (c == 'w')
		Keyboard_Publish(KEY_UP, stamp_ms);
	else if (c == 's')
		Keyboard_Publish(KEY_DOWN, stamp_ms);
	else if (c == 'a')
		Keyboard_Publish(KEY_LEFT, stamp_ms);
	else if (c == 'd')
		Keyboard_Publish(KEY_RIGHT, stamp_ms);
	else if (c == 'r')
		Keyboard_Publish(KEY_RESTART, stamp_ms);
}

//******************************************************************************************
// Function: Keyboard_Decode
//
// Purpose:
//      Decode one byte while remembering any unfinished arrow sequence
//
// Inputs:
//      c - next byte taken from the receive queue
//      stamp_ms - receive time saved with that byte
//
// Output:
//      None
//******************************************************************************************
static void Keyboard_Decode(char c, uint32_t stamp_ms)
{
	// The octal escape \033 is ESC, which starts a terminal key sequence
	// A new ESC starts a new sequence even if an earlier one was unfinished
	if (c == '\033')
	{
		s_parse = PARSE_ESCAPE;
		s_escape_ms = stamp_ms;
		return;
	}

	// Without an escape prefix, try the ordinary letter mappings
	if (s_parse == PARSE_NORMAL)
	{
		Keyboard_Letter(c, stamp_ms);
		return;
	}

	// Accept the two supported arrow prefixes, ESC [ and ESC O
	// An unrecognized prefix byte is consumed without making a letter command
	if (s_parse == PARSE_ESCAPE)
	{
		s_parse = (c == '[') ? PARSE_CSI : ((c == 'O') ? PARSE_SS3 : PARSE_NORMAL);
		return;
	}

	// After an expired ESC, consume the prefix before looking for a final byte
	// Otherwise a late [ or O would end discard mode and the arrow could become a letter
	if (s_parse == PARSE_DISCARD_ESCAPE)
	{
		s_parse = (c == '[' || c == 'O') ? PARSE_DISCARD : PARSE_NORMAL;
		return;
	}

	// A, B, C and D finish the supported Up, Down, Right and Left sequences
	// The final byte supplies the command timestamp
	if (s_parse == PARSE_CSI || s_parse == PARSE_SS3)
	{
		s_parse = PARSE_NORMAL;
		if (c == 'A')
			Keyboard_Publish(KEY_UP, stamp_ms);
		else if (c == 'B')
			Keyboard_Publish(KEY_DOWN, stamp_ms);
		else if (c == 'C')
			Keyboard_Publish(KEY_RIGHT, stamp_ms);
		else if (c == 'D')
			Keyboard_Publish(KEY_LEFT, stamp_ms);
		// Parameter bytes belong to an unsupported sequence
		// Discard its remaining bytes so a trailing letter cannot become a drive command
		else if ((uint8_t)c < 0x40U || (uint8_t)c > 0x7EU)
			s_parse = PARSE_DISCARD;
		return;
	}

	// In discard mode, consume bytes until a final byte in the range @ through ~
	if ((uint8_t)c >= 0x40U && (uint8_t)c <= 0x7EU)
		s_parse = PARSE_NORMAL;
}

//******************************************************************************************
// Function: Keyboard_ApplyRxFault
//
// Purpose:
//      Clear unreliable input and prepare an input-error event
//
// Inputs:
//      None, the caller must have interrupts paused
//
// Output:
//      None
//******************************************************************************************
static void Keyboard_ApplyRxFault(void)
{
	// The caller pauses interrupts so the ISR cannot change this fault snapshot
	if (!s_rx_fault)
		return;

	// Empty both queues and forget any partial arrow after lost or damaged input
	s_rx_tail = s_rx_head;
	s_event_tail = s_event_head;
	s_rx_fault = false;
	s_resync = true;
	s_parse = PARSE_NORMAL;

	// Prepare one error event and wait for quiet before accepting new bytes
	s_error_pending = true;
	s_error_ms = s_activity_ms;
}

//******************************************************************************************
// Function: Keyboard_PopByte
//
// Purpose:
//      Take one complete byte record from the ISR receive queue
//
// Inputs:
//      byte - address where the received byte and its time will be copied
//
// Output:
//      True when a record was copied
//      False when input is paused or no byte is available
//******************************************************************************************
static bool Keyboard_PopByte(Rx_Byte *byte)
{
	// Briefly protect the byte and timestamp copy from an ISR update
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	// Do not return queued input while a fault or recovery pause is active
	bool ready = !s_rx_fault && !s_resync && s_rx_tail != s_rx_head;
	if (ready)
	{
		// Copy one complete record before advancing the tail
		*byte = s_rx[s_rx_tail];
		s_rx_tail = (s_rx_tail + 1U) % KEYBOARD_RX_SIZE;
	}

	// Restore the caller's IRQ state and return without waiting for another byte
	__set_PRIMASK(primask);
	return ready;
}

//******************************************************************************************
// Function: Keyboard_Service
//
// Purpose:
//      Decode a limited number of bytes in the foreground and service recovery
//
// Inputs:
//      now_ms - caller time, refreshed before checking the final timeout
//
// Output:
//      None
//******************************************************************************************
void Keyboard_Service(uint32_t now_ms)
{
	// Handle shared recovery flags with interrupts briefly paused
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	Keyboard_ApplyRxFault();

	// Use the current clock so activity recorded by the ISR is not newer than this time
	uint32_t current_ms = timebase_get_ms();

	// Every discarded byte restarts the quiet interval
	// Unsigned elapsed time still works when the millisecond counter wraps
	if (s_resync && (uint32_t)(current_ms - s_activity_ms) >= KEYBOARD_QUIET_MS)
		s_resync = false;
	__set_PRIMASK(primask);

	// Decode at most the configured budget and return to the other services
	// The next call can continue with any bytes left in the queue
	for (uint32_t i = 0; i < KEYBOARD_RX_BUDGET; i++)
	{
		Rx_Byte byte;
		if (!Keyboard_PopByte(&byte))
			break;
		// Use receive times to decide whether the remaining arrow bytes arrived in time
		// On timeout, remember whether the prefix still needs to be consumed
		if ((s_parse == PARSE_ESCAPE || s_parse == PARSE_CSI || s_parse == PARSE_SS3) && (uint32_t)(byte.timestamp_ms - s_escape_ms) > KEYBOARD_ESCAPE_MS)
			s_parse = (s_parse == PARSE_ESCAPE) ? PARSE_DISCARD_ESCAPE : PARSE_DISCARD;
		Keyboard_Decode(byte.value, byte.timestamp_ms);
	}

	// Refresh time because new bytes may have arrived after the caller sampled it
	// Also expire an unfinished sequence when no more bytes arrive
	now_ms = timebase_get_ms();
	if ((s_parse == PARSE_ESCAPE || s_parse == PARSE_CSI || s_parse == PARSE_SS3) && (uint32_t)(now_ms - s_escape_ms) > KEYBOARD_ESCAPE_MS)
		s_parse = (s_parse == PARSE_ESCAPE) ? PARSE_DISCARD_ESCAPE : PARSE_DISCARD;
}

//******************************************************************************************
// Function: Keyboard_TakeEvent
//
// Purpose:
//      Give foreground application code one keyboard command or input-error event
//
// Inputs:
//      event - address where the event and its receive time will be copied
//
// Output:
//      True when an event was copied
//      False when the pointer is null or no event is available
//******************************************************************************************
bool Keyboard_TakeEvent(Keyboard_Event *event)
{
	// The caller must provide somewhere to copy the event
	if (!event)
		return false;

	// A UART fault may have arrived after Keyboard_Service, so check it here too
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	Keyboard_ApplyRxFault();
	bool ready = false;

	// Return the error before any normal command and consume this notification once
	// The application decides how to stop and recover
	if (s_error_pending)
	{
		event->command = KEY_INPUT_ERROR;
		event->timestamp_ms = s_error_ms;
		s_error_pending = false;
		ready = true;
	}
	// Otherwise return the oldest decoded command with its original receive time
	else if (s_event_head != s_event_tail)
	{
		*event = s_events[s_event_tail];
		s_event_tail = (s_event_tail + 1U) % KEYBOARD_EVENT_SIZE;
		ready = true;
	}

	// Restore the caller's IRQ state whether or not an event was available
	__set_PRIMASK(primask);
	return ready;
}

//******************************************************************************************
// Function: Keyboard_CommandName
//
// Purpose:
//      Provide a readable command name for the display
//
// Inputs:
//      command - keyboard command to describe
//
// Output:
//      Pointer to the command name or UNKNOWN
//******************************************************************************************
const char *Keyboard_CommandName(Keyboard_Command command)
{
	// Keep this table in the same order as Keyboard_Command in keyboard.h
	static const char * const names[] = {"UP", "DOWN", "LEFT", "RIGHT", "RESTART", "INPUT_ERROR"};
	// Check the range before using the command as an array index
	return (unsigned int)command < sizeof(names) / sizeof(names[0]) ? names[command] : "UNKNOWN";
}
