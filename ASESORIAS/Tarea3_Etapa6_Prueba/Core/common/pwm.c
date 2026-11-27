/* core/common/pwm.c — controla los tres colores del RGB con TIM3 a 1 kHz */
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "pwm.h"
#include "system.h"

#define PWM_TICK_HZ       1000000U      // hacer que el contador avance a 1 MHz: 1 tick equivale a 1 us
#define PWM_PERIOD_TICKS  1000U         // contar 1000 ticks produce 1 ms de periodo, es decir 1 kHz

static TIM_HandleTypeDef htim_pwm;      // manejador para configurar TIM3 y su canal PWM
static bool pwm_started = false;       
static uint32_t pwm_duty[3] = {0U, 0U, 0U}; // último porcentaje de cada color

/* calcula el reloj real del timer considerando el divisor configurado en APB1 */
static uint32_t pwm_timer_clock_hz(void);
static uint32_t pwm_get_timer_channel(pwm_color_t color);

/* configura PC6, PC8 y PC9 como canales PWM de TIM3 con duty inicial de 0 % */
void pwm_init(void)
{
    GPIO_InitTypeDef gpio = {0};         
    TIM_OC_InitTypeDef channel = {0};    

    /* 1. señal de reloj para GPIOC y TIM3 */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* 2. PC6, PC8 y PC9 usan AF02 para los canales 1, 3 y 4 de TIM3 */
    gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;       // función alternativa - pushpull
    gpio.Pull      = GPIO_NOPULL;           // sin pull up-down
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;   // baja velocidad
    gpio.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* 3. base de tiempo: contar a 1 MHz hasta 999, así un periodo dura 1 ms */
    htim_pwm.Instance               = TIM3; // TIM3
    htim_pwm.Init.Prescaler         = (pwm_timer_clock_hz() / PWM_TICK_HZ) - 1U; // dividir el timer a 1 MHz
    htim_pwm.Init.CounterMode       = TIM_COUNTERMODE_UP; // conteo ascendente
    htim_pwm.Init.Period            = PWM_PERIOD_TICKS - 1U; // 1000 ticks por periodo
    htim_pwm.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1; // sin división 
    htim_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE; // precarga activada
    if (HAL_TIM_PWM_Init(&htim_pwm) != HAL_OK) {
        Error_Handler();
    }

    /* 4. configurar los tres canales con salida activa en nivel alto */
    channel.OCMode     = TIM_OCMODE_PWM1;       // modo PWM 1
    channel.Pulse      = 0U;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;   // activo en nivel alto
    channel.OCFastMode = TIM_OCFAST_DISABLE;    
    if (HAL_TIM_PWM_ConfigChannel(&htim_pwm, &channel, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_ConfigChannel(&htim_pwm, &channel, TIM_CHANNEL_3) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_ConfigChannel(&htim_pwm, &channel, TIM_CHANNEL_4) != HAL_OK) {
        Error_Handler();
    }

    /* 5. iniciar cada canal PWM */
    if (HAL_TIM_PWM_Start(&htim_pwm, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_Start(&htim_pwm, TIM_CHANNEL_3) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_Start(&htim_pwm, TIM_CHANNEL_4) != HAL_OK) {
        Error_Handler();
    }
    pwm_started = true;                         // desde ahora pwm_set_duty() puede cambiar el canal
}

/*
cambia el ciclo de trabajo. Retorna false si el valor está fuera de rango o si la
salida nunca se inició (pwm_init() no se llamó en esta aplicación)
*/
bool pwm_set_duty(pwm_color_t color, uint32_t percent)
{
    uint32_t timer_channel;

    /* no escribir el comparador si falta inicializar el timer o los datos no son válidos */
    if ((!pwm_started) || (percent > 100U) || (color > PWM_BLUE)) {
        return false;
    }
    timer_channel = pwm_get_timer_channel(color);
    pwm_duty[color] = percent;
    __HAL_TIM_SET_COMPARE(&htim_pwm, timer_channel, percent * (PWM_PERIOD_TICKS / 100U));
    return true;                                // el nuevo ciclo de trabajo quedó programado
}

/* retorna el ciclo de trabajo establecido por última vez, en porcentaje */
uint32_t pwm_get_duty(pwm_color_t color)
{
    if (color > PWM_BLUE) {
        return 0U;
    }
    return pwm_duty[color];
}

/* relaciona cada color con su canal de TIM3 */
static uint32_t pwm_get_timer_channel(pwm_color_t color)
{
    switch (color) {
    case PWM_RED:
        return TIM_CHANNEL_1;
    case PWM_GREEN:
        return TIM_CHANNEL_3;
    case PWM_BLUE:
    default:
        return TIM_CHANNEL_4;
    }
}

/* TIM3 está en APB1: su reloj es PCLK1, o 2 x PCLK1 cuando el prescaler de APB1 no es 1 */
static uint32_t pwm_timer_clock_hz(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();    // leer la frecuencia del bus APB1

    if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) {
        return pclk1;                           // aPB1 sin división: TIM3 usa PCLK1
    }
    return 2U * pclk1;                          // aPB1 dividido: el reloj de TIM3 es 2 x PCLK1
}
