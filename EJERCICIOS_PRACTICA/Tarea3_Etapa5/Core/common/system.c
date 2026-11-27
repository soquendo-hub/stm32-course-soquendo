/* core/common/system.c — configuración común de reloj y manejo de errores fatales */
#include "stm32f4xx_hal.h"
#include "system.h"

/* configura el HSI de 16 MHz como reloj del sistema y deja los buses sin división */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0}; // parámetros para elegir y configurar el oscilador
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0}; // parámetros para repartir el reloj entre buses

   /* hSI está disponible después del reset; seleccionar su configuración interna */
   RCC_OscInitStruct.OscillatorType       = RCC_OSCILLATORTYPE_HSI;     // oscilador HSI (reloj interno de alta velocidad)
   RCC_OscInitStruct.HSIState             = RCC_HSI_ON;                 // hSI encendido
   RCC_OscInitStruct.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT; // valor de calibración predeterminado
   RCC_OscInitStruct.PLL.PLLState         = RCC_PLL_NONE;               // no se usa PLL
   HAL_RCC_OscConfig(&RCC_OscInitStruct);           // pedir a HAL que aplique la configuración del oscilador

   /* seleccionar HSI como SYSCLK y configurar los divisores de los buses */
   RCC_ClkInitStruct.ClockType            = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                            RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2; // indica que se va a configurar (SYSCLK, HCLK, PCLK1 y PCLK2)
   RCC_ClkInitStruct.SYSCLKSource         = RCC_SYSCLKSOURCE_HSI;                       // hSI como fuente del reloj principal
   RCC_ClkInitStruct.AHBCLKDivider        = RCC_SYSCLK_DIV1;      // hCLK queda igual que SYSCLK: 16 MHz
   RCC_ClkInitStruct.APB1CLKDivider       = RCC_HCLK_DIV1;        // aPB1 = 16 MHz
   RCC_ClkInitStruct.APB2CLKDivider       = RCC_HCLK_DIV1;        // aPB2 = 16 MHz

   /* fLASH_LATENCY_0 = cero estados de espera, corecto para 16 MHz */
   HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0); // aplicar los relojes y la latencia de Flash
}

/* se llama cuando una inicialización crítica falla y no es seguro seguir ejecutando */
void Error_Handler(void)
{
    __disable_irq();     /* detener interrupciones; el heartbeat congelado hace visible el fallo */
    while (1) {
        /* permanecer aquí permite localizar el error con el depurador */
    }
}
