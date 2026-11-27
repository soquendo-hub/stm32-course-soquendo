/* core/common/pwm.h — interfaz de los canales PWM RGB de TIM3 */
#ifndef PWM_H
#define PWM_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PWM_RED,
    PWM_GREEN,
    PWM_BLUE
} pwm_color_t;

/* configura los pines y el timer; las salidas comienzan con 0 % de duty */
void     pwm_init(void);
/* aplica un porcentaje entre 0 y 100; devuelve false si no puede aplicarlo */
bool     pwm_set_duty(pwm_color_t color, uint32_t percent);
/* devuelve el porcentaje que se configuró por última vez */
uint32_t pwm_get_duty(pwm_color_t color);

#endif
