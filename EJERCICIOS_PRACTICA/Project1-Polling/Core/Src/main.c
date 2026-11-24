/*
 * main.c
 * Project1-Polling: Project 1 reads the analog voltage on PB0 twice
 * per second and prints it. Each time, the main loop starts one
 * conversion, waits until the ADC finishes, reads the result and
 * sends it over USART2.
 * Autor: Steven Arley Oquendo Rave
 */

 #include "Legacy/stm32_hal_legacy.h"
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_gpio_ex.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_rcc_ex.h"
 #include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_uart.h"
#include <stdint.h>
#include <sys/_intsup.h>

 /* Handle ser globlar para que stm32f4xx_it.c pueda accederlo */
 TIM_HandleTypeDef  htim4;
 UART_HandleTypeDef huart2;
 volatile uint8_t send_msg_flag = 0;

 /* Prototipos de funciones privadas */
 static void SystemClock_Config (void);
 static void gpio_Init(void);
 static void tim4_Init(void);
 static void usart2_Init(void);

 int main(void)
 {
    HAL_Init();            // inicializa HAL: SysTick, caché, agrupación de prioridades
    SystemClock_Config();  // configura el árbol de reloj: HSI a 16 MHz                
    gpio_Init();           // configura PH1 como salida push-pull                      
    tim4_Init();           // configura TIM4: evento de actualización cada 250 ms
    usart2_Init();         // configura PA2 para TX y PA3 para RX

    while (1)
    {
      if (send_msg_flag) 
      {
         send_msg_flag = 0;
         char msg[] = "Hello from STM32\r\n";
         HAL_UART_Transmit(&huart2, (uint8_t *)msg, sizeof(msg) - 1, 100);

      }
    }
 }

 /*
  * SystemClock_Config
  * Usa el oscilador interno HSI a 16 MHz
  * Sin PLL - la configuración de reloj más simple posible
  */

static void SystemClock_Config(void)
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

/*
 * gpio_Init
 * Configura PH1 como salida push-pull - LED integrado en la placa Nucleo
 */
 static void gpio_Init(void)
 {
   GPIO_InitTypeDef GPIO_InitStruct = {0};

   /* Habilita el reloj de GPIOH en el bus AHB1
      Igual que en bare-metal: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN */
      __HAL_RCC_GPIOH_CLK_ENABLE();

   /* Configura PH1 */
   GPIO_InitStruct.Pin     = GPIO_PIN_1;
   GPIO_InitStruct.Mode    = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull    = GPIO_NOPULL;
   GPIO_InitStruct.Speed   = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
 }

 /*
  * tim4_Init
  * Configura TIM4 para generar un evento de actualización cada 250 ms
  *
  * Cadena de reloj:
  *   HSI (16 MHz) -> APB1 (16 MHz) -> reloj de TIM4 (16 MHz)
  *
  * PSC = 15999 -> tick = 16 000 000 /(15 999 + 1) = 1 000 Hz (1 ms por tick)
  * ARR = 249   -> periodo = (249 +1) * 1 ms = 250 ms
  */
  static void tim4_Init(void)
  {
   /* Habilita el reloj de TIM4 en el bus APB1 */
   __HAL_RCC_TIM4_CLK_ENABLE();

   /* Configura la base de TIM4 */
   htim4.Instance                = TIM4;
   htim4.Init.Prescaler          = 15999;
   htim4.Init.CounterMode        = TIM_COUNTERMODE_UP;
   htim4.Init.Period             = 249;
   htim4.Init.ClockDivision      = TIM_CLOCKDIVISION_DIV1;
   htim4.Init.AutoReloadPreload  = TIM_AUTORELOAD_PRELOAD_DISABLE;
   
   HAL_TIM_Base_Init(&htim4);

   /* Inicia TIM4 en modo interrución - habilita la interrución del evento de actualización */
   HAL_TIM_Base_Start_IT(&htim4);

   /* Habilita la línea de interrupción de TIM4 en el NVIC*/
   HAL_NVIC_EnableIRQ(TIM4_IRQn);
  }

  static void usart2_Init(void)
  {
   GPIO_InitTypeDef GPIO_InitStruct = {0};

   /* Paso 1: habilita los relojes - tanto USART2 como GPIOA */
   __HAL_RCC_USART2_CLK_ENABLE();
   __HAL_RCC_GPIOA_CLK_ENABLE();    // ya se llama en gpio_Init() - es seguro llamarla de nuevo

   /* Paso 2: configura PA2 (TX) y PA3 (RX) como función alterna
      AF7 es USART2 en el STM32F411 - ver la tabla de funciones alterna en el datasheet */
      GPIO_InitStruct.Pin           = GPIO_PIN_2 | GPIO_PIN_3;
      GPIO_InitStruct.Mode          = GPIO_MODE_AF_PP;
      GPIO_InitStruct.Pull          = GPIO_NOPULL;
      GPIO_InitStruct.Speed         = GPIO_SPEED_FREQ_LOW;
      GPIO_InitStruct.Alternate     = GPIO_AF7_USART2;
      HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

   /* Paso3 3: configura USART2 */
   huart2.Instance                  = USART2;
   huart2.Init.BaudRate             = 115200;
   huart2.Init.WordLength           = UART_WORDLENGTH_8B;
   huart2.Init.StopBits             = UART_STOPBITS_1;
   huart2.Init.Parity               = UART_PARITY_NONE;
   huart2.Init.Mode                 = UART_MODE_TX_RX;
   huart2.Init.HwFlowCtl            = UART_HWCONTROL_NONE;
   huart2.Init.OverSampling         = UART_OVERSAMPLING_16;
   HAL_UART_Init(&huart2);
  }

  /* 
   * HAL_TIM_PeriodElapsedCallback
   * Llamada automáticamente por HAL_TIM_IRQHandler() cada vez que se dispara
   * un evento de actualización de un temporizador. Compartida por todos los
   * temporizadores - siempre verifica htim->Instance.
   */
   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
   {
      if (htim->Instance == TIM4) 
      {
         HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_1);
         send_msg_flag = 1;
      }
   }