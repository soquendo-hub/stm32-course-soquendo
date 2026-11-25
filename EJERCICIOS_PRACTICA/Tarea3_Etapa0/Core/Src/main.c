/*
 * @file    main.c
 * @author  Steven Arley Oquendo Rave
 * @brief   Punto de entrada común de las etapas de la tarea UART.
 *          Inicializa HAL, el reloj y el heartbeat; después delega el
 *          comportamiento a la aplicación seleccionada por APP en CMake.
 *          En la etapa de preparación, la aplicación todavía no usa UART.
 */

#include "stm32f4xx_hal.h"
#include "system.h"
#include "heartbeat.h"
#include "app.h"

int main(void)
{
    /* Inicializa los servicios básicos de HAL, incluido el tick de SysTick. */
    HAL_Init();

    /* Configura HSI y los divisores de los relojes del sistema y los buses. */
    SystemClock_Config();

    /* Inicia el heartbeat compartido; PH1 cambia cada 250 ms (parpadeo de 2 Hz). */
    heartbeat_init(250U);

    /* Inicializa una sola vez la aplicación que APP seleccionó en CMake. */
    app_init();

    while (1) {
        /* Ejecuta continuamente el ciclo de la aplicación seleccionada. */
        app_loop();
    }
}
   
