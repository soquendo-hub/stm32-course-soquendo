/* Core/common/system.c */
#include "stm32f4xx_hal.h"
#include "system.h"

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

   /* HSI ya está encendido tras el reset - se confirma y se usa */
   RCC_OscInitStruct.OscillatorType       = RCC_OSCILLATORTYPE_HSI;
   RCC_OscInitStruct.HSIState             = RCC_HSI_ON;
   RCC_OscInitStruct.HSICalibrationValue  = RCC_HSICALIBRATION_DEFAULT;
   RCC_OscInitStruct.PLL.PLLState         = RCC_PLL_NONE;
   HAL_RCC_OscConfig(&RCC_OscInitStruct);

   /* Selecciona HSI como SYSCLK - todos los divisores de bus en 1 */
   RCC_ClkInitStruct.ClockType            = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                            RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
   RCC_ClkInitStruct.SYSCLKSource         = RCC_SYSCLKSOURCE_HSI;
   RCC_ClkInitStruct.AHBCLKDivider        = RCC_SYSCLK_DIV1;      // HCLK = 16 MHz
   RCC_ClkInitStruct.APB1CLKDivider       = RCC_HCLK_DIV1;        // APB1 = 16 MHz
   RCC_ClkInitStruct.APB2CLKDivider       = RCC_HCLK_DIV1;        // APB2 = 16 MHz

   /* FLASH_LATENCY_0 = cero estados de espera, corecto para 16 MHz */
   HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

void Error_Handler(void)
{
    __disable_irq();     /* detiene toda interrupción: el heartbeat se congela, un "CPU atascada" visible */
    while (1) {
        /* quédate aquí; el depurador muestra dónde terminamos */
    }
}
