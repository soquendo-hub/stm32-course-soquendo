/*
 * @file    main.c
 * @author  Steven Arley Oquendo Rave
 * @brief   Tarea3_Etapa2: ensambla lineas y ejecuta los primeros comandos: help, info, blink <ms> y echo <text>
 */

#include "stm32f4xx_hal.h"
#include "system.h"
#include "heartbeat.h"
#include "app.h"

int main(void)
{
    HAL_Init();                             //Inicializa los servición de HAL

    SystemClock_Config();                   //configura HSI y los relojes

    heartbeat_init(250U);       //Inicia el Blinky

    app_init();                             //Inicializa la app seleccionada

    while (1) {
        app_loop();                         //Ejecuta la app seleccionada en cada ciclo del while loop
    }
}
   
