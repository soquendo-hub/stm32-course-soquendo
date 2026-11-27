/*
 * stm32f4xx_it.c
 * Rutinas de servicio de interrupción
 * Autor: tu nombre
 */

#include "stm32f4xx_hal.h"

/* Declara el handle de TIM4 — definido en main.c */
extern TIM_HandleTypeDef htim4;

/* Manejador de SysTick — requerido por HAL para HAL_Delay() y los timeouts */
void SysTick_Handler(void)
{
    HAL_IncTick();
}


