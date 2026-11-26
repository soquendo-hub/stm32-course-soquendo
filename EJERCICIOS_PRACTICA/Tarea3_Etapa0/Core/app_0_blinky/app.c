#include "stm32f4xx_hal.h"
#include "heartbeat.h"
#include "system.h"

void app_init(void)
{
    HAL_Delay(5000U);
    heartbeat_set_period(500U);
    HAL_Delay(5000U);
    Error_Handler();
}

void app_loop(void)
{
    /* nada: el heartbeat corre desde la interrupción de TIM4 */
}