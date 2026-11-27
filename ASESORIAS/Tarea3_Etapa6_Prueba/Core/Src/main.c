/*

* @file    main.c
* @author  steven Arley Oquendo Rave
* @brief   arranque común: configura HAL, reloj y heartbeat, luego ejecuta la app seleccionada

*/

#include "stm32f4xx_hal.h"
#include "system.h"
#include "heartbeat.h"
#include "app.h"

int main(void)
{
    HAL_Init();                             // preparar HAL antes de usar los periféricos STM32

    SystemClock_Config();                   // seleccionar HSI y configurar los relojes de sistema y buses

    heartbeat_init(250U);                   // iniciar el heartbeat, que cambia de estado cada 250 ms

    app_init();                             // inicializar la aplicación elegida por el selector CMake

    while (1) {
        app_loop();                         // dar a la aplicación una oportunidad de procesar eventos continuamente
    }
}
   
