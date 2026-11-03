/*
 * servo.c
 * Phase 1 - one servo to be used only, on D5 / PB4
 *
 * Created on: Oct 5, 2026
 *      Author: Mitesh Parikh
 */
#include "servo.h"
#include "project_config.h"
#include "stm32l476xx.h"

// RCC Register Masks
#define GPIOBEN_MASK (0x00000002)
#define TIM3EN_MASK (0x00000002)

// Timer Register Masks
#define CEN_MASK (0x0001)
#define CC1E_MASK (0x0001)
#define CC1IE_MASK (0x0002)

// Timer values
#define SERVO_TIMER_HZ  (1000000U)   // 1 count = 1 us
#define SERVO_FRAME_US  (20000U)     // 20 ms frame = 50 Hz

// Random garbage
#define ALTERNATE_FUNCTION_MODE (0x02) // this isn't in binary so it's 1 0000 which isn't what we want was (0x10) changing to (0x02)
#define ALTERNATE_FUNCTION_2 (0x02) // same mistake with this one
#define MEDIUM_SPEED (0x01)

// will not lie the #define was just something I just did for the say of consistency
#define POSITION_INCREMENT2 ((SERVO_MAX_US-SERVO_MIN_US) / SERVO_LAST_POSITION) // there are 5 intervals and 6 positions, idk why we were using 6

static const uint16_t POSITIONS2[SERVO_LAST_POSITION + 1] =
{
	SERVO_MIN_US, // 0 -> 1000us
	SERVO_MIN_US + (POSITION_INCREMENT2 * 1), // 1000 -> 1200us
	SERVO_MIN_US + (POSITION_INCREMENT2 * 2), // 200
	SERVO_MIN_US + (POSITION_INCREMENT2 * 3),
	SERVO_MIN_US + (POSITION_INCREMENT2 * 4),
	SERVO_MAX_US
};

//******************************************************************************************
// Set up the servo output on PB4 / TIM3_CH1
// Work out the prescaler, frame length and starting pulse from the timer clock
// Use the reference manual to choose the GPIO and timer settings
// Keep the output off until the starting values are ready
//******************************************************************************************
void Servo_Init(void)
{
	// Add your initialization here

	// Frame length = 20ms
	// PS = 79 - Don't change
	// ARR = 19,999 - Don't change
	// CCR1 := high time in us, don't use -1 like PS/ARR, needs 1000us-2000us for the servo
	// GPIO output should use AF2 (0010)
	// gpio moder 10 for pb4

	// 1. EN TIM3
	RCC->APB1ENR1 |= TIM3EN_MASK;
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

	// 2. Stop counting and disable channel outputs
	TIM3->CR1  = 0;   // CEN = 0, DIR = 0, CMS = 00, ARPE = 0
	TIM3->CCER = 0;   // channel outputs off
	TIM3->DIER = 0;   // no interrupts or DMA requests
	TIM3->SMCR = 0;   // no slave mode, internal clock

	// 3. Prepare time base, PS/ARR/CNT
	TIM3->PSC = (PROJECT_CLOCK_HZ / SERVO_TIMER_HZ) - 1;   // 80 MHz / 80 = 1 MHz
	TIM3->ARR = SERVO_FRAME_US - 1;                        // 20000 counts = 20 ms
	TIM3->CNT = 0;

	// 4. Channel 1 as PWM mode 1, enable CCR1 and ARR preload, initial CCR1
	TIM3->CCMR1 &= ~(TIM_CCMR1_CC1S);    // CC1S = 00 -> output
	TIM3->CCMR1 &= ~(TIM_CCMR1_OC1M);    // clears bits 6:4 and bit 16
	TIM3->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1; // OC1M = 0110 -> PWM mode 1
	TIM3->CCMR1 |= TIM_CCMR1_OC1PE;      // buffer CCR1 until the next update
	TIM3->CR1 |= TIM_CR1_ARPE;         // buffer ARR until the next update
	TIM3->CCR1 = POSITIONS2[0];        // start at position 0 (SERVO_MIN_US)

	// 5. Load the starting values, then clear the startup flag
	TIM3->EGR = TIM_EGR_UG;
	TIM3->SR = 0; // we can't set this flag to 1 ever, that is hardware's job we can only set it to 0. our old code for this works, but could cause bugs in the future and this is just more simple

	// 6. Prepare PB4 route and driver, AF2/Pushpull/Medium speed/no pulls, MODER4=10
	// Enable GPIOB Clock Access
	// only PB4 fields are changed

	// Configure alternate function mode
	GPIOB->AFR[0] &= ~(GPIO_AFRL_AFSEL4);
	GPIOB->AFR[0] |= (ALTERNATE_FUNCTION_2 << GPIO_AFRL_AFSEL4_Pos); // this should work now

	// Push-Pull output mode
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT4);

	// Set port output speed to medium
	GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED4);
	GPIOB->OSPEEDR |= (MEDIUM_SPEED << GPIO_OSPEEDR_OSPEED4_Pos);

	// No Pull-Up/Down
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD4);

	// this is set last in the block diagram which is why I'm following right now
	GPIOB->MODER &= ~(GPIO_MODER_MODE4);
	GPIOB->MODER |= (ALTERNATE_FUNCTION_MODE << GPIO_MODER_MODE4_Pos); // this should be 10 now and not 1 0000

	// 7. EN Channel with active high, CC1E=1, CC1P=0, CC1NP=0
	TIM3->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP);
	TIM3->CCER |= TIM_CCER_CC1E;

	// 8. CEN to start counting last, CR1 bit 0
	TIM3->CR1 |= CEN_MASK;
}

//******************************************************************************************
// Change the pulse width without changing the frame length
// Check the request before touching the timer
// Use preload so a new pulse starts cleanly at a frame boundary
//******************************************************************************************
bool Servo_SetPulseUs(uint32_t pulse_us)
{
	// If the given pulse is within the range
	if (pulse_us >= SERVO_MIN_US && pulse_us <= SERVO_MAX_US)
	{
		TIM3->CCR1 = pulse_us;
		return true;
	}
	else
	{
		return false;
	}
}

//******************************************************************************************
// Map one of the six positions to a pulse width
// Think about the two endpoints and what happens for an invalid position
//******************************************************************************************
bool Servo_SetPosition(uint8_t position)
{
	bool isValidPosition = false;

	// Ensure position is in the bounds of the array
	// I don't think we need the position >= 0 anymore but I'm keeping it since I don't want to break anything anymore XD
	if(position >= 0 && position < ARRAY_LENGTH(POSITIONS2))
	{
		isValidPosition = Servo_SetPulseUs(POSITIONS2[position]);
	}

	return isValidPosition;
}
