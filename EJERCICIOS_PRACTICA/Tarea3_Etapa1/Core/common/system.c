/* Core/common/system.c */
#include "stm32f4xx_hal.h"
#include "system.h"

void SystemClock_Config(void)   //prepara la configuración del reloj y la entrega a HAL
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};     //Variable de configuración de osciladores inicia en cero
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};     //Variable de configuración de relojes inicia en cero

   /* HSI ya está encendido tras el reset - se confirma y se usa */
   RCC_OscInitStruct.OscillatorType       = RCC_OSCILLATORTYPE_HSI;     //Oscilador HSI (reloj interno de alta velocidad)
   RCC_OscInitStruct.HSIState             = RCC_HSI_ON;                 //HSI encendido
   RCC_OscInitStruct.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT; //Valor de calibración predeterminado
   RCC_OscInitStruct.PLL.PLLState         = RCC_PLL_NONE;               //No se usa PLL
   HAL_RCC_OscConfig(&RCC_OscInitStruct);           //se entregan los aprametros a HAL para que los aplique

   /* Selecciona HSI como SYSCLK - todos los divisores de bus en 1 */
   RCC_ClkInitStruct.ClockType            = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                            RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2; //Indica que se va a configurar (SYSCLK, HCLK, PCLK1 y PCLK2)
   RCC_ClkInitStruct.SYSCLKSource         = RCC_SYSCLKSOURCE_HSI;                       //HSI como fuente del reloj principal
   RCC_ClkInitStruct.AHBCLKDivider        = RCC_SYSCLK_DIV1;      // HCLK queda igual que SYSCLK: 16 MHz
   RCC_ClkInitStruct.APB1CLKDivider       = RCC_HCLK_DIV1;        // APB1 = 16 MHz
   RCC_ClkInitStruct.APB2CLKDivider       = RCC_HCLK_DIV1;        // APB2 = 16 MHz

   /* FLASH_LATENCY_0 = cero estados de espera, corecto para 16 MHz */
   HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);    //se entregan los aprametros a HAL para que los aplique
}

void Error_Handler(void)
{
    __disable_irq();     /* detiene toda interrupción: el heartbeat se congela, un "CPU atascada" visible */
    while (1) {
        /* quédate aquí; el depurador muestra dónde terminamos */
    }
}
