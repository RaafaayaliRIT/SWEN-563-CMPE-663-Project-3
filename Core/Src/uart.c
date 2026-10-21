/*
 * uart.c
 *
 *      Created on: Jan 21, 2026
 *      Author: Mitesh Parikh
 *      Comments: Bare-metal UART module, using USART2 of STM32L476 MCU
 *
 *      Updated on: Oct 5, 2026
 *      Comments: Added queued TX, RX error notification and Project 3 IRQ priority
 */


//
//	Hardware mapping:
//	- USART2 TX on PA2 (Alternate Function AF7)
//	- USART2 RX on PA3 (Alternate Function AF7)
//
//	Clock assumption:
//	- APB1 (PCLK1) = 80 MHz
//
//	References:
//	1) STM32L476 Reference Manual (RM0351):
//	 - RCC: AHB2ENR (GPIO clocks), APB1ENR1 (USART2 clock)
//	 - GPIO: MODER, AFRL (alternate function selection)
//	 - USART: CR1, BRR, ISR, RDR, TDR
//	2) NUCLEO-L476RG User Manual (UM1724):
//	 - ST-LINK Virtual COM Port connection to USART2
//

#include "uart.h"


// GPIO mapping for USART2
#define UART_TX_PIN                2U      // PA2
#define UART_RX_PIN                3U      // PA3
#define UART_AF_USART2             7U      // AF7 selects USART2 on PA2/PA3

// APB1 clock frequency -- must match SysClock.c
#define UART_PCLK1_HZ              80000000U

// We will Set for Alternate function in GPIO so we can select USART2 on PA2 / PA3
// From Reference Manual: GPIOx_MODER: 00=input, 01=output, 10=AF, 11=analog
//
#define GPIO_MODE_ALTFN            0x2U

// Project 3 TX queue: one foreground producer, USART2 ISR consumer
// One slot remains empty to distinguish a full queue from an empty queue
#define UART_TX_BUFFER_SIZE        512U

static char s_tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint32_t s_tx_head = 0;
static volatile uint32_t s_tx_tail = 0;
static volatile bool s_irq_enabled = false;

//******************************************************************************************
// Initialize USART2 for serial communication at the specified baud rate
//******************************************************************************************
void USART_Init(uint32_t baud)
{
    // ----------------------------------------------------------
    // Step 1 - Enable peripheral clocks
    // ----------------------------------------------------------
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;         // GPIOA clock enable
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;       // USART2 clock enable

    // ----------------------------------------------------------
    // Step 2 -  Configure GPIOA pins PA2/PA3 for USART2 (AF7)
    //
    // MODER:
    //  - each pin uses 2 bits: 00=input, 01=output, 10=AF, 11=analog
    // AFRL:
    //  - pins 0..7 use AFR[0], 4 bits per pin select AF number
    // ----------------------------------------------------------

    // Set PA2 = Alternate Function mode
    GPIOA->MODER &= ~(3U << (2U * UART_TX_PIN));
    GPIOA->MODER |=  (GPIO_MODE_ALTFN << (2U * UART_TX_PIN));

    // Set PA3 = Alternate Function mode
    GPIOA->MODER &= ~(3U << (2U * UART_RX_PIN));
    GPIOA->MODER |=  (GPIO_MODE_ALTFN << (2U * UART_RX_PIN));

    // Select AF7 for PA2 and PA3 (USART2)
    GPIOA->AFR[0] &= ~(0xFU << (4U * UART_TX_PIN));
    GPIOA->AFR[0] |=  (UART_AF_USART2 << (4U * UART_TX_PIN));

    GPIOA->AFR[0] &= ~(0xFU << (4U * UART_RX_PIN));
    GPIOA->AFR[0] |=  (UART_AF_USART2 << (4U * UART_RX_PIN));

    // GPIO electrical settings (defaults for UART):
    // - Push-pull TX
    // - Medium speed TX
    // - No pulls
    //
    GPIOA->OTYPER  &= ~(1U << UART_TX_PIN);                 // push-pull
    GPIOA->OSPEEDR &= ~(3U << (2U * UART_TX_PIN));
    GPIOA->OSPEEDR |=  (1U << (2U * UART_TX_PIN));  		// TX speed = medium
    GPIOA->PUPDR   &= ~(3U << (2U * UART_TX_PIN));          // no pull on TX
    GPIOA->PUPDR   &= ~(3U << (2U * UART_RX_PIN));          // no pull on RX

    // ----------------------------------------------------------
    // Step 3: Configure USART2
    // ----------------------------------------------------------

    // Disable USART while configuring BRR/CR1
    USART2->CR1 &= ~(USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_TXEIE);
    USART2->CR3 &= ~USART_CR3_EIE;

    // Project 3: initialize the TX queue once during startup
    s_tx_head = 0;
    s_tx_tail = 0;
    s_irq_enabled = false;
    USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF;

    // Set BRR for desired baud rate:
    //   BRR = Clock / baud rate
    // 		Using integer rounding to reduce error
    //
    USART2->BRR = (UART_PCLK1_HZ + (baud / 2U)) / baud;

    // Enable transmitter and receiver (8 data bits, no parity, 1 stop bit are defaults)
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE);

    // Enable USART
    USART2->CR1 |= USART_CR1_UE;
}

//******************************************************************************************
// Transmit a single character (blocking)
//******************************************************************************************
void USART_Write(char c)
{
    // TXE = transmit data register empty
    while (!(USART2->ISR & USART_ISR_TXE)) { }

    USART2->TDR = (uint8_t)c;
}

//******************************************************************************************
// Transmit a null-terminated string (blocking)
//******************************************************************************************
void USART_Write_String(const char *str)
{
    // If nothing to write then return
	if (!str)
    	return;

	// Write entire string
    while (*str)
    {
        USART_Write(*str++);
    }
}

//******************************************************************************************
// Read a character (blocking)
//******************************************************************************************
char USART_Read(void)
{
    // RXNE = receive data register not empty
    while (!(USART2->ISR & USART_ISR_RXNE)) { }

    return (char)USART2->RDR; // Note: reading RDR clears RXNE
}


//******************************************************************************************
// Read a character without blocking; returns 0 if none available
//******************************************************************************************
char USART_Read_NB(USART_TypeDef *USARTx)
{
    if (USARTx && (USARTx->ISR & USART_ISR_RXNE))
    {
        return (char)USARTx->RDR;
    }
    return 0;
}


//******************************************************************************************
//******************************************************************************************
// For IRQ Support
//******************************************************************************************
//******************************************************************************************

static void (*s_rx_callback)(char) = 0;
static void (*s_rx_error_callback)(uint32_t) = 0;

//******************************************************************************************
// Copy a whole message into the TX queue without waiting for the UART to send it
// The USART2 ISR sends the queued bytes as the transmitter becomes ready
// This lets the main loop keep servicing the car, sensor, servo and button
// True means the message was copied into the queue, not that sending has finished
// False means nothing was queued, so try later or skip this update
// Call from the foreground after enabling IRQ mode
// Let the main loop continue instead of retrying in a waiting loop
// The supplied display calls this for normal screen updates
// Do not mix queued output with blocking writes or direct TDR writes such as printf
//******************************************************************************************
bool USART_TryWrite(const char *data, size_t length)
{
	// An empty message succeeds without sending anything
	if (length == 0)
		return true;

	// A nonempty message needs valid data and IRQ support
	if (!data || !s_irq_enabled)
		return false;

	// Head is where we add bytes and tail is where the ISR takes them
	// Keep one slot free to distinguish an empty queue from a full queue
	uint32_t head = s_tx_head;
	uint32_t tail = s_tx_tail;
	uint32_t free_bytes = (tail + UART_TX_BUFFER_SIZE - head - 1U) % UART_TX_BUFFER_SIZE;

	// Check space for the whole message before copying any bytes
	if (length > free_bytes)
		return false;

	// Copy into free slots and wrap around at the end of the buffer
	// The ISR can send older bytes because we have not moved the shared head yet
	for (size_t i = 0; i < length; i++)
	{
		s_tx_buffer[head] = data[i];
		head = (head + 1U) % UART_TX_BUFFER_SIZE;
	}

	// Briefly pause interrupts to publish the message and enable TX service
	// Make the copied bytes visible before moving the head and restore the IRQ mask
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	__DMB();
	s_tx_head = head;
	USART2->CR1 |= USART_CR1_TXEIE;
	__set_PRIMASK(primask);

	return true;
}

//******************************************************************************************
// Register a callback invoked when USART2 receives a byte (in IRQ mode)
//******************************************************************************************
void USART2_RegisterRxCallback(void (*cb)(char))
{
    s_rx_callback = cb;
}

//******************************************************************************************
// Register a callback for RX errors; errors contains USART_ISR_ORE, FE and/or NE bits
// The callback runs in the USART2 ISR and must only record the error for the application
//******************************************************************************************
void USART2_RegisterRxErrorCallback(void (*cb)(uint32_t))
{
	s_rx_error_callback = cb;
}

//******************************************************************************************
// Enable USART2 RX/error interrupts and queued TX support in USART and NVIC
//******************************************************************************************
void USART2_EnableIRQ(void)
{
	// Project 3: keep USART2 below B1 (priority 0) and planned sensor capture (priority 1).
	NVIC_SetPriority(USART2_IRQn, 2);
	USART2->CR3 |= USART_CR3_EIE;
	s_irq_enabled = true;

    // RXNEIE enables an interrupt when RXNE (receive data ready) is set
    USART2->CR1 |= USART_CR1_RXNEIE;

    // Enable USART2 interrupt in NVIC
    NVIC_EnableIRQ(USART2_IRQn);
}

//******************************************************************************************
// USART2 interrupt handler (RX, RX errors and queued TX)
//******************************************************************************************
void USART2_IRQHandler(void)
{
    uint32_t status = USART2->ISR;
    uint32_t errors = status & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE);

	// Clear observed RX errors and discard the associated byte instead of executing it.
	if (errors)
	{
		if (status & USART_ISR_RXNE)
			(void)USART2->RDR;
		uint32_t clear_flags = 0;
		if (errors & USART_ISR_ORE)
			clear_flags |= USART_ICR_ORECF;
		if (errors & USART_ISR_FE)
			clear_flags |= USART_ICR_FECF;
		if (errors & USART_ISR_NE)
			clear_flags |= USART_ICR_NCF;
		USART2->ICR = clear_flags;
		if (s_rx_error_callback)
			s_rx_error_callback(errors);
	}

    // RXNE set means a byte is ready in RDR
	// NOte: reading RDR clears RXNE
    if (!errors && (status & USART_ISR_RXNE))
    {
        // read data and save
    	char c = (char)USART2->RDR;
        if (s_rx_callback)
        {
            s_rx_callback(c);
        }
    }

	// Service at most one queued byte per interrupt; disable TXEIE when the queue is empty.
	if ((USART2->CR1 & USART_CR1_TXEIE) && (USART2->ISR & USART_ISR_TXE))
	{
		if (s_tx_tail != s_tx_head)
		{
			USART2->TDR = (uint8_t)s_tx_buffer[s_tx_tail];
			s_tx_tail = (s_tx_tail + 1U) % UART_TX_BUFFER_SIZE;
		}
		if (s_tx_tail == s_tx_head)
			USART2->CR1 &= ~USART_CR1_TXEIE;
	}
}

