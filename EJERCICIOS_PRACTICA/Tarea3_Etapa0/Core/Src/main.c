/*
 * main.c
 * Tarea3_Etapa0_Preparación
 * Objetivos:
 * -  Compliar y enlazar el driver HAL UART.
 * -  Construir un archivo de aplicación
 * por etapa para que desde un main.c fijo 
 * se seleccione la aplicación mediante 
 * la variable APP.
 * -  Blinky (TIM4 + PH1) y configuración
 * del reloj viven en COre/common/.
 * -  Hablar con la placa desde una 
 * terminal terminal serial en el PC.
 * Autor: Steven Arley Oquendo Rave
 */

/* Core/Src/main.c — igual para todas las etapas; no se vuelve a editar */
#include "stm32f4xx_hal.h"
#include "system.h"
#include "heartbeat.h"
#include "app.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    heartbeat_init(250U);   /* cambia de estado cada 250 ms = parpadeo a 2 Hz */
    app_init();             /* provista por la etapa seleccionada */

    while (1) {
        app_loop();         /* provista por la etapa seleccionada */
    }
}
   