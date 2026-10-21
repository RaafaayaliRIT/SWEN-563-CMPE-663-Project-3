/*
 * Uart.h
 *
 *  Created on: Jan 21, 2026
 *      Author: Mitesh Parikh
 *
 */
#ifndef NUCLEO476_UART_H_
#define NUCLEO476_UART_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "stm32l476xx.h"


// Initialize USART2 for serial communication at the specified baud rate
void USART_Init(uint32_t baud);

// Transmit one character (blocking code)
void USART_Write(char c);

// Transmit a null-terminated string (blocking code)
void USART_Write_String(const char *str);

// Copy a whole message into the TX queue without waiting for the UART to send it
// The USART2 ISR sends the queued bytes as the transmitter becomes ready
// This lets the main loop keep servicing the car, sensor, servo and button
// True means the message was copied into the queue, not that sending has finished
// False means nothing was queued, so try later or skip this update
// Call from the foreground after enabling IRQ mode
// Let the main loop continue instead of retrying in a waiting loop
// The supplied display calls this for normal screen updates
// Do not mix queued output with blocking writes or direct TDR writes such as printf
bool USART_TryWrite(const char *data, size_t length);

// Read one character (blocking; waits until RXNE)
char USART_Read(void);

// Read one character without blocking; returns 0 if none available
char USART_Read_NB(USART_TypeDef *USARTx);

// Enable RX/error interrupts and queued TX support; call after registering callbacks
// In IRQ mode use callbacks for RX and USART_TryWrite() for TX instead of blocking/polling APIs
void USART2_EnableIRQ(void);

// USART2 interrupt handler
void USART2_IRQHandler(void);

// Register before enabling IRQ mode; callback runs in ISR context and must remain short
void USART2_RegisterRxCallback(void (*cb)(char));

// Register before enabling IRQ mode; errors contains USART_ISR_ORE, FE and/or NE bits
// ISR callback must only record the error; the application resynchronizes its RX queue/parser
void USART2_RegisterRxErrorCallback(void (*cb)(uint32_t));

#endif /* NUCLEO476_UART_H_ */
