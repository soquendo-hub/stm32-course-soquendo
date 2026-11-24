/* Core/common/heartbeat.c */
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "heartbeat.h"
#include "stm32f4xx_hal_gpio.h"
#include "system.h"

#define HB_TIM       TIM4
#define HB_TICK_HZ   10000U        /*10 kHz: 1 tick = 0,1 ms */
#define HB_LED_PORT  GPIOH
#define HB_LED_PIN   GPIO_PIN_1    /* PH1 */

static TIM_HandleTypeDef htim_hb;  /* static: solo este archivo puede tocarlo */
static volatile uint32_t hb_period_ms;

/* TIM4 está en APB1: su reloj es PCLK1, o 2 x PCLK1 cuando el prescaler de APB1 no es 1 */
static uint32_t hb_timer_clock_hz(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) {
        return pclk1;
    }
    return 2U * pclk1;
}

void heartbeat_init(uint32_t toggle_ms)
{
    GPIO_InitTypeDef gpio = {0};

    /* 1. Relojes: GPIOH para el LED, TIM4 para el temporizador */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    /* 2. Pin del LED */
    gpio.Pin   = HB_LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(HB_LED_PORT, &gpio);

    /* 3. Registros del temporizador */
    hb_period_ms = toggle_ms;
    htim_hb.Instance               = HB_TIM;
    htim_hb.Init.Prescaler         = (hb_timer_clock_hz() / HB_TICK_HZ) - 1U;
    htim_hb.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim_hb.Init.Period            = (toggle_ms * (HB_TICK_HZ / 1000U)) - 1U;
    htim_hb.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim_hb.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim_hb) != HAL_OK) {
        Error_Handler();
    }

    /* 4. Interrupción: primero el NVIC, luego arranca el temporizador con su interrupción de actualización */
    HAL_NVIC_SetPriority(TIM4_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    if (HAL_TIM_Base_Start_IT(&htim_hb) != HAL_OK) {
        Error_Handler();
    }
}

bool heartbeat_set_period(uint32_t toggle_ms)
{
    if ((toggle_ms < HEARTBEAT_MIN_MS) || (toggle_ms > HEARTBEAT_MAX_MS)) {
        return false;                  /* rechazar: el heartbeat debe seguir siendo visible */
    }
    hb_period_ms = toggle_ms;
    __HAL_TIM_SET_AUTORELOAD(&htim_hb, (toggle_ms * (HB_TICK_HZ / 1000U)) - 1U);
    return true;
}

uint32_t heartbeat_get_period(void)
{
    return hb_period_ms;
}

/* ---- Manejador de interrupción y callback de la HAL (funciones débiles sobrescritas aquí) ---- */
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim_hb);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == HB_TIM) {
        HAL_GPIO_TogglePin(HB_LED_PORT, HB_LED_PIN);
    }
}
