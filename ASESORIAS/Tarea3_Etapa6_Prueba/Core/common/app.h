#ifndef APP_H
#define APP_H

/* inicializa la aplicación elegida, después de HAL, el reloj y el heartbeat */
void app_init(void);
/* ejecuta el trabajo repetido de la aplicación desde el while infinito de main() */
void app_loop(void);

#endif
