/**
 ******************************************************************************
 * @file    main.c
 * @brief   Alterna el LED de la placa usando TIM2 y su interrupción.
 * @author  Soquendo
 ******************************************************************************
 */

#include "stm32f4xx.h"

#define LED_PIN 5U

static void gpio_InitLed(void);
static void tim2_Init(void);

int main(void)
{
	gpio_InitLed();
	tim2_Init();

	while (1)
	{
		/* TIM2 alterna el LED desde su rutina de interrupción. */
	}
}

void TIM2_IRQHandler(void)
{
	if ((TIM2->SR & TIM_SR_UIF) != 0U)
	{
		TIM2->SR &= ~TIM_SR_UIF;
		GPIOA->ODR ^= (1U << LED_PIN);
	}
}

static void gpio_InitLed(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	GPIOA->MODER &= ~(0x3U << (LED_PIN * 2U));
	GPIOA->MODER |= (0x1U << (LED_PIN * 2U));
}

static void tim2_Init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

	TIM2->PSC = 15999U;
	TIM2->ARR = 499U;
	TIM2->DIER |= TIM_DIER_UIE;

	NVIC_EnableIRQ(TIM2_IRQn);
	TIM2->CR1 |= TIM_CR1_CEN;
}
