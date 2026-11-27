/* Core/common/heartbeat.h */
#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdbool.h>
#include <stdint.h>

#define HEARTBEAT_MIN_MS    50U
#define HEARTBEAT_MAX_MS  5000U

void     heartbeat_init(uint32_t toggle_ms);
bool     heartbeat_set_period(uint32_t toggle_ms);  /* lo usa el comando "blink" en la Etapa 2 */
uint32_t heartbeat_get_period(void);

#endif
