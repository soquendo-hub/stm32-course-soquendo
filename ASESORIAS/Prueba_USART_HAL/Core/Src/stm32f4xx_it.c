/**
 * @file    stm32f4xx_it.c
 * @author  Steven Arley Oquendo Rave
 * @brief   Handlers de interrupción STM32F411 que despachan eventos a HAL.
 */

#include "stm32f4xx_hal.h"

/* Handles definidos en main.c y despachados por los IRQ de HAL. */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;
extern UART_HandleTypeDef huart2;

/* Manejador de SysTick — requerido por HAL para HAL_Delay() y los timeouts */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* Manejador del evento de actualización de TIM4 */
void TIM4_IRQHandler(void)
{
    //Baja la bandera (escribiendo 0) para que la interrupción se pueda repetir
    HAL_TIM_IRQHandler(&htim4);
}

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

void EXTI0_IRQHandler(void)
{
    //Se baja escribiendo 1 (en EXTI->PR se limpia con 1, no con 0)
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}
