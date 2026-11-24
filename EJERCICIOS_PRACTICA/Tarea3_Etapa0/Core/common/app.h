#ifndef APP_H
#define APP_H

void app_init(void);   /* se llama una vez, después de que el reloj y el heartbeat están corriendo */
void app_loop(void);   /* se llama para siempre desde el bucle while (1) de main() */

#endif