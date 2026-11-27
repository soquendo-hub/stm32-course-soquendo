/* core/common/pwm.c — TIM3 canal 1 en PC6 , 1 kHz, duty 0..100 % */
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "pwm.h"
#include "system.h"

#define PWM_TICK_HZ       1000000U      // hacer que el contador avance a 1 MHz: 1 tick equivale a 1 us
#define PWM_PERIOD_TICKS  1000U         // contar 1000 ticks produce 1 ms de periodo, es decir 1 kHz

static TIM_HandleTypeDef htim_pwm;      // HAL usa este handle para configurar TIM3 y su canal PWM
static bool pwm_started = false;       // evita escribir el comparador antes de iniciar el PWM
static uint32_t pwm_duty = 0U;          // último porcentaje aplicado correctamente

/* calcula el reloj real del timer considerando el divisor configurado en APB1 */
static uint32_t pwm_timer_clock_hz(void);

/*
configura el canal 1 de TIM3 como salida PWM de 1 kHz en PC6 y la inicia en 0 %
el mismo orden que todo periférico: relojes, pin, temporizador, canal, inicio
*/
void pwm_init(void)
{
    GPIO_InitTypeDef gpio = {0};         // ajustes de PC6 para conectarlo a TIM3_CH1
    TIM_OC_InitTypeDef channel = {0};    // ajustes del modo PWM y del valor inicial del canal

    /* 1. encender los relojes: GPIOC para PC6 y TIM3 para generar la señal */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* 2. conectar PC6 a la función alternativa 2, que en este pin es TIM3_CH1 */
    gpio.Pin       = GPIO_PIN_6;            // seleccionar el pin 6 del puerto C
    gpio.Mode      = GPIO_MODE_AF_PP;       // el pin entrega la señal del periférico en modo push-pull
    gpio.Pull      = GPIO_NOPULL;           // no añadir resistencias pull-up ni pull-down internas
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;   // la velocidad baja es suficiente para una señal PWM de 1 kHz
    gpio.Alternate = GPIO_AF2_TIM3;         // conectar el pin con la función alternativa de TIM3
    HAL_GPIO_Init(GPIOC, &gpio);

    /* 3. base de tiempo: contar a 1 MHz hasta 999, así un periodo dura 1 ms */
    htim_pwm.Instance               = TIM3; // elegir TIM3 como fuente de la señal periódica
    htim_pwm.Init.Prescaler         = (pwm_timer_clock_hz() / PWM_TICK_HZ) - 1U; // dividir el timer hasta 1 MHz
    htim_pwm.Init.CounterMode       = TIM_COUNTERMODE_UP; // contar desde cero hacia arriba
    htim_pwm.Init.Period            = PWM_PERIOD_TICKS - 1U; // contar de 0 a 999: 1000 ticks por periodo
    htim_pwm.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1; // no dividir adicionalmente el reloj del timer
    htim_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE; // aplicar el periodo nuevo al actualizar
    if (HAL_TIM_PWM_Init(&htim_pwm) != HAL_OK) {
        Error_Handler();
    }

    /* 4. en PWM1 la salida está activa mientras CNT sea menor que el valor Pulse */
    channel.OCMode     = TIM_OCMODE_PWM1;       // seleccionar el modo PWM 1 del canal
    channel.Pulse      = 0U;                    // empezar en 0 %: el LED queda apagado
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;   // la parte activa del pulso sale en nivel alto
    channel.OCFastMode = TIM_OCFAST_DISABLE;    // no usar el modo rápido de comparación
    if (HAL_TIM_PWM_ConfigChannel(&htim_pwm, &channel, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }

    /* 5. iniciar TIM3 canal 1; el periférico genera PWM por hardware sin ISR de actualización */
    if (HAL_TIM_PWM_Start(&htim_pwm, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    pwm_started = true;                         // desde ahora pwm_set_duty() puede cambiar el canal
}

/*
cambia el ciclo de trabajo. Retorna false si el valor está fuera de rango o si la
salida nunca se inició (pwm_init() no se llamó en esta aplicación)
*/
bool pwm_set_duty(uint32_t percent)
{
    /* no escribir el comparador si falta inicializar el timer o el porcentaje no es válido */
    if ((!pwm_started) || (percent > 100U)) {
        return false;
    }
    pwm_duty = percent;                         // guardar lo aplicado para que pwm_get_duty() lo devuelva
    __HAL_TIM_SET_COMPARE(&htim_pwm, TIM_CHANNEL_1, percent * (PWM_PERIOD_TICKS / 100U)); // 1 % = 10 ticks
    return true;                                // el nuevo ciclo de trabajo quedó programado
}

/* retorna el ciclo de trabajo establecido por última vez, en porcentaje */
uint32_t pwm_get_duty(void)
{
    return pwm_duty;                            // el valor está expresado en porcentaje, de 0 a 100
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
