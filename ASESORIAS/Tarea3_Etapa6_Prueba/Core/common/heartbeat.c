/* core/common/heartbeat.c — controla el LED heartbeat mediante TIM4 y PH1 */
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "heartbeat.h"
#include "stm32f4xx_hal_gpio.h"
#include "system.h"

#define HB_TIM       TIM4           
#define HB_TICK_HZ   10000U         
#define HB_LED_PORT  GPIOH          
#define HB_LED_PIN   GPIO_PIN_1     

static TIM_HandleTypeDef htim_hb;  
static volatile uint32_t hb_period_ms; 

/* TIM4 está en APB1: su reloj es PCLK1, o 2 x PCLK1 cuando el prescaler de APB1 no es 1 */
static uint32_t hb_timer_clock_hz(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq(); // leer el reloj configurado para el bus APB1
    if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) {
        return pclk1;                      // sin divisor APB1, el timer recibe PCLK1 directamente
    }
    return 2U * pclk1;                     // con divisor APB1, los timers reciben el doble de PCLK1
}

/* configura el pin y el temporizador, y comienza a alternar el LED cada toggle_ms */
void heartbeat_init(uint32_t toggle_ms)
{
    GPIO_InitTypeDef gpio = {0};

    /* 1. relojes: GPIOH para el LED, TIM4 para el temporizador */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    /* 2. configurar PH1 como salida digital para poder cambiar su nivel */
    gpio.Pin   = HB_LED_PIN;                 // PH1
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;        // salida push-pull
    gpio.Pull  = GPIO_NOPULL;                // no pull up-down
    gpio.Speed = GPIO_SPEED_FREQ_LOW;        // baja velocidad
    HAL_GPIO_Init(HB_LED_PORT, &gpio);

    /* 3. convertir el periodo pedido a valores de prescaler y auto-reload */
    hb_period_ms = toggle_ms;              // guardar el periodo para que info/blink puedan consultarlo
    htim_hb.Instance               = HB_TIM; // elegir TIM4 como temporizador del heartbeat
    htim_hb.Init.Prescaler         = (hb_timer_clock_hz() / HB_TICK_HZ) - 1U; // 10 kHz
    htim_hb.Init.CounterMode       = TIM_COUNTERMODE_UP; // conteo ascendente
    htim_hb.Init.Period            = (toggle_ms * (HB_TICK_HZ / 1000U)) - 1U; // periodo en ms
    htim_hb.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1; // sin división
    htim_hb.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE; // activar precarga
    if (HAL_TIM_Base_Init(&htim_hb) != HAL_OK) {
        Error_Handler();
    }

    /* 4. permitir que el NVIC atienda TIM4 y después iniciar el timer con interrupciones */
    HAL_NVIC_SetPriority(TIM4_IRQn, 3, 0);  // asignar prioridad a la interrupción de TIM4
    HAL_NVIC_EnableIRQ(TIM4_IRQn);           // permitir que el NVIC atienda la interrupción
    if (HAL_TIM_Base_Start_IT(&htim_hb) != HAL_OK) {
        Error_Handler();
    }
}

bool heartbeat_set_period(uint32_t toggle_ms)
{
    /* rechazar periodos que hagan demasiado rápido o imperceptible el heartbeat */
    if ((toggle_ms < HEARTBEAT_MIN_MS) || (toggle_ms > HEARTBEAT_MAX_MS)) {
        return false;                  
    }
    hb_period_ms = toggle_ms;             // actualizar el valor que devuelve heartbeat_get_period()
    __HAL_TIM_SET_AUTORELOAD(&htim_hb, (toggle_ms * (HB_TICK_HZ / 1000U)) - 1U); // cambiar el periodo del timer
    return true;
}

uint32_t heartbeat_get_period(void)
{
    return hb_period_ms;                  // devolver el periodo guardado en milisegundos
}

/* ---- manejador de interrupción y callback de la HAL (funciones débiles sobrescritas aquí) ---- */
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim_hb);         // dejar que HAL identifique y limpie el evento del temporizador
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == HB_TIM) {       // este callback también podría recibir eventos de otros timers
        HAL_GPIO_TogglePin(HB_LED_PORT, HB_LED_PIN); // invertir PH1 para mostrar que el programa sigue vivo
    }
}
