#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_intsup.h>
#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "app.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_gpio_ex.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_uart.h"
#include "system.h"

#define TX_TIMEOUT_MS  100U     //El mayor tiempo que aceptamos para esperar por una transmisión
#define RX_TIMEOUT_MS  10U      //Qué tanto espera app_loop() por un byte
#define TICK_PERIOD_MS 1000U    //Una linea tick por segundo

static UART_HandleTypeDef huart2;   //static: solo este archivo lo usa

static void uart2_init(void);
static void uart2_send_text(const char *text);

/*se ejecuta una sola vez al inicio despues del reloj y el bliky
    Configura USART2 e imprime el banner*/
    void app_init(void)
{
    uart2_init();
    uart2_send_text("\r\nStage 1: polling. Type something.\r\n");
}

/*Se ejecuta en cada ciclo del while loop. Imprime una linea tick
    cada segundo, mientras tanto espera RX_TIMEOUT_MS (10ms) por cada byte
    y lo describe. Mientras esté esperando dentro de HAL_UART_Receive()
    no corre nada más*/
 
void app_loop(void)
{
    static uint32_t last_tick_ms = 0U;      //tiempo del ultimo tick
    static uint32_t tick_count = 0U;        //contador de ticks
    uint8_t byte_aux;                       //Byte auxiliar
    char msg[40];

    /* 1. Una vez por segundo, mostrar que el bucle está vivo, y cuándo */
    if ((HAL_GetTick() - last_tick_ms) >= TICK_PERIOD_MS) {
        last_tick_ms = HAL_GetTick();
        tick_count++;
        (void)snprintf(msg, sizeof msg, "tick %lu at %lu ms\r\n",
                       (unsigned long)tick_count, (unsigned long)last_tick_ms);
        uart2_send_text(msg);
    }

    /*2. Esperar hasta RX_TIMEOUT_MS por cada byte, y luego describirlo*/
    if (HAL_UART_Receive(&huart2, &byte_aux, 1u, RX_TIMEOUT_MS) == HAL_OK) {
        char shown = ((byte_aux >= 0x20U) && (byte_aux < 0x7FU)) ? (char)byte_aux : '.';
        (void)snprintf(msg, sizeof msg, "rx: '%c' 0x%02X\r\n", shown, byte_aux);
        uart2_send_text(msg);
    
    }
}

/*Configurar USART2: 115200, 8N1 on PA2/PA3, 
    en el mismo orden que heartbeat_init(): relojes, pines, periféricos*/
static void uart2_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* 1. Relojes: USART2 (APB1) y GPIOA*/
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 2. PINES: PA2 = TX, PA3 = RX, función alternativa AF7 para ambos*/
    gpio.Pin                = GPIO_PIN_2 | GPIO_PIN_3;  //PA2/PA3
    gpio.Mode               = GPIO_MODE_AF_PP;          //FUNCIÓN ALTERNATIVA
    gpio.Pull               = GPIO_PULLUP;              //DESCANSA EN ALTO
    gpio.Speed              = GPIO_SPEED_FREQ_HIGH;     //HIGH SPEED
    gpio.Alternate          = GPIO_AF7_USART2;           //AF7
    HAL_GPIO_Init(GPIOA, &gpio);

    /*3. USART2: 115200 baudios, 8N1, trasmición y recepción, sin control de flujo*/
    huart2.Instance         = USART2;
    huart2.Init.BaudRate    = 115200u;              //115200 BAUDIOS
    huart2.Init.WordLength  = UART_WORDLENGTH_8B;   //8 bits
    huart2.Init.StopBits    = UART_STOPBITS_1;      //un bit de parada
    huart2.Init.Parity      = UART_PARITY_NONE;     //sin paridad
    huart2.Init.Mode        = UART_MODE_TX_RX;      //transmición y recepción
    huart2.Init.HwFlowCtl   = UART_HWCONTROL_NONE;  //sin control de flujo
    huart2.Init.OverSampling= UART_OVERSAMPLING_16; //16 muestras de lectura    
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }

    /* 4. NVIC: NADA: POLLING NO USA INTERRUPCIONES*/
}

/*  Enviar un string a C y se espera mientras no se haya ido (Blqueante)
    Un mensake perdido no es fatal aquí, por lo que el estado se ignora a proposito*/
static void uart2_send_text(const char *text)
{
    (void)HAL_UART_Transmit(&huart2, (const uint8_t *)
                            text,(uint16_t)strlen(text), TX_TIMEOUT_MS);
}