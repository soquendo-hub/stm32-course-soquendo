/* core/common/pwm.h — interfaz del PWM de TIM3 canal 1 en PC6 */
#ifndef PWM_H
#define PWM_H

#include <stdbool.h>
#include <stdint.h>

/* configura el pin y el timer; la salida comienza con 0 % de ciclo de trabajo */
void     pwm_init(void);
/* aplica un porcentaje entre 0 y 100; devuelve false si no puede aplicarlo */
bool     pwm_set_duty(uint32_t percent);
/* devuelve el porcentaje que se configuró por última vez */
uint32_t pwm_get_duty(void);

#endif
