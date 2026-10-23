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
#define TIMER_FREQUENCY_HZ (1000000)
#define FRAME_LENGTH (20000)

// Random garbage
#define TIM_CCMR1_OC1M_PWM_MODE1 (0x00000006)
#define ALTERNATE_FUNCTION_MODE (0x10)
#define ALTERNATE_FUNCTION_2 (0x0010)
#define MEDIUM_SPEED (0x01)

const uint32_t POSITION_INCREMENT2 = (SERVO_MAX_US-SERVO_MIN_US / 6);
const uint16_t POSITIONS2[] =
{
	SERVO_MIN_US,
	SERVO_MIN_US + POSITION_INCREMENT2 * 1,
	SERVO_MIN_US + POSITION_INCREMENT2 * 2,
	SERVO_MIN_US + POSITION_INCREMENT2 * 3,
	SERVO_MIN_US + POSITION_INCREMENT2 * 4,
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

	// 2. Stop counting and disable channel outputs
	TIM3->CR1 &= ~(CEN_MASK); // Disable counter
	TIM3->CR1 &= ~(TIM_CR1_DIR); // Direction Up
	TIM3->CR1 &= ~(TIM_CR1_CMS); // Use DIR
	TIM3->CCER &= ~(CC1E_MASK); // Disable capture compare for channel 1
	TIM3->DIER &= ~(CC1IE_MASK); // Disable DMA interrupt for channel 1
	TIM3->SMCR &= ~(TIM_SMCR_SMS | TIM_SMCR_SMS_3); // Disable slave mode

	// 3. Prepare time base, PS/ARR/CNT
	TIM3->PSC = (PROJECT_CLOCK_HZ - TIMER_FREQUENCY_HZ) / TIMER_FREQUENCY_HZ;
	TIM3->ARR = ((uint32_t) FRAME_LENGTH * (uint32_t) TIMER_FREQUENCY_HZ) - 1;
	TIM3->CNT = 0;

	// 4. Prepare CH1 for TIM3 as PWM mode 1, EN CCR1/ARR preload, CCR1 = 1000
	TIM3->CCMR1 &= ~(TIM_CCMR1_CC1S);
	TIM3->CCMR1 &= ~(TIM_CCMR1_OC1M_3); // Why is this not next to the rest, who designed this???
	TIM3->CCMR1 |= TIM_CCMR1_OC1M_PWM_MODE1 | TIM_CCMR1_OC1PE;
	TIM3->CR1 |= TIM_CR1_ARPE;
	TIM3->CCR1 = POSITIONS2[0];

	// 5. Load starting values, ERG.UG=1 then clear startup flags
	TIM3->EGR |= TIM_EGR_UG;
	TIM3->SR &= ~(TIM_SR_UIF);

	// 6. Prepare PB4 route and driver, AF2/Pushpull/Medium speed/no pulls, MODER4=10
	// Enable GPIOB Clock Access
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

	// Configure alternate function mode
	GPIOB->MODER &= ~(GPIO_MODER_MODE4);
	GPIOB->MODER |= (ALTERNATE_FUNCTION_MODE << GPIO_MODER_MODE4_Pos);
	GPIOB->AFR[0] &= ~(GPIO_AFRL_AFSEL4);
	GPIOB->AFR[0] |= (ALTERNATE_FUNCTION_2 << GPIO_AFRL_AFSEL4_Pos);

	// Push-Pull output mode
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT4);

	// Set port output speed to medium
	GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED4);
	GPIOB->OSPEEDR |= (MEDIUM_SPEED << GPIO_OSPEEDR_OSPEED4_Pos);

	// No Pull-Up/Down
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD4);

	// 7. EN Channel with active high, CC1E=1, CC1P=0, CC1NP=0
	TIM3->CCER |= TIM_CCER_CC1E;
	TIM3->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP);

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
	if(position >= 0 && position < ARRAY_LENGTH(POSITIONS2))
	{
		isValidPosition = Servo_SetPulseUs(POSITIONS2[position]);
	}

	return isValidPosition;
}
