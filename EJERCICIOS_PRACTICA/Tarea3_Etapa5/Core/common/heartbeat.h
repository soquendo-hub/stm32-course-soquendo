/* core/common/heartbeat.h */
#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdbool.h>
#include <stdint.h>

#define HEARTBEAT_MIN_MS    50U   /* periodo mínimo aceptado por heartbeat_set_period() */
#define HEARTBEAT_MAX_MS  5000U   /* periodo máximo aceptado por heartbeat_set_period() */

/* configura el heartbeat y empieza con el periodo indicado en milisegundos */
void     heartbeat_init(uint32_t toggle_ms);
/* cambia el periodo; devuelve false si queda fuera de los límites definidos arriba */
bool     heartbeat_set_period(uint32_t toggle_ms);
/* devuelve el último periodo aceptado, en milisegundos */
uint32_t heartbeat_get_period(void);

#endif
