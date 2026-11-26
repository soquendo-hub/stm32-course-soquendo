/* Core/app_2_interrupt/app.c — Etapa 2: recepción por interrupción */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_intsup.h>
#include "stm32f4xx_hal.h"
#include "app.h"
#include "stm32f4xx_hal_uart.h"
#include "system.h"
#include "cmd_parser.h"
#include "commands.h"


#define TX_TIMEOUT_MS  100U
#define RX_RING_SIZE   64U      //(RX_RING_SIZE - 1U) devuelve un índice a 0 al llegar al final.
#define LINE_MAX_LEN   63u      //La linea de comando más larga sin el end mark (\0)

static UART_HandleTypeDef huart2;
static uint8_t rx_byte;                         //HAL almacena cada byte recibido
static uint8_t rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head = 0U;          // escrito solo por la interrupción
static volatile uint16_t rx_tail = 0U;          // escrito solo por app_loop()
static volatile uint32_t rx_error_count = 0U;
static volatile uint32_t rx_last_error = 0U;
static char line[LINE_MAX_LEN + 1U];            //El arreglo necesita un byte más que el texto más largo que vaya a contener.
static uint16_t line_len = 0U;
static bool line_too_long = false;

static void uart2_init(void);
static void uart2_start_rx(void);
static void uart2_send_text(const char *text);
static void rx_ring_put(uint8_t byte_to_put);
static bool rx_ring_get(uint8_t *byte_to_get);
static void line_add_byte(uint8_t new_byte);
static void line_execute(void);



/* Corre una vez al arrancar, después del reloj y el heartbeat.
   Configura USART2, imprime el banner e inicia la primera recepción. */
void app_init(void)
{
    uart2_init();
    uart2_send_text("\r\nStage 2: interrupt reception.\r\n> ");
    uart2_start_rx();
}

/* Se ejecuta en cada ciclo del while loop desde main(). Se toma cada byte 
   almacenado en la interrupción del ring y lo imprime; luego reporta cualquier
   error de recepción. */
void app_loop(void)
{
    static uint32_t reported_errors = 0U;
    uint8_t byte_aux;
    char msg[64];

    /* 1. Entregar cada byte a la línea en vez de imprimirlo */
    while (rx_ring_get(&byte_aux)) 
    {
        line_add_byte(byte_aux);
    }


    /* 2. Reportar los errores de recepción vistos por la interrupción */
    if (rx_error_count != reported_errors) 
    {
        reported_errors = rx_error_count;
        (void)snprintf(msg, sizeof msg, "\r\n[uart error 0x%02lX, reception restarted]\r\n",
                       (unsigned long)rx_last_error);
        uart2_send_text(msg);
    }
}

/*Agrega un byte recibido a la línea que se está escribiendo, y le hace eco.
    Enter (\r, \n p \r\n) termina la línea y ejecuta line_execute(). */
static void line_add_byte(uint8_t new_byte)
{
    static uint8_t previous_byte = 0U;

    if ((new_byte == '\n') && (previous_byte == '\r')) {
        previous_byte = new_byte;
        return;                                 /* segunda mitad de "\r\n": ya se manejó */
    }
    previous_byte = new_byte;

    if ((new_byte == '\r') || (new_byte == '\n')) {    /* fin de línea */
        uart2_send_text("\r\n");
        line_execute();
        uart2_send_text("> ");
    } else if (line_len < LINE_MAX_LEN) {
        line[line_len] = (char)new_byte;
        line_len++;
        (void)HAL_UART_Transmit(&huart2, &new_byte, 1U, TX_TIMEOUT_MS);   /* hacerle eco */
    } else {
        line_too_long = true;                   /* seguir leyendo, pero rechazar la línea */
    }
}

/* Convierte la línea terminada en una cadena C, la analiza y ejecuta el comando,
   y luego vacía la línea para la siguiente. Las líneas vacías se ignoran. */
static void line_execute(void)
{
    cmd_t cmd;
    char reply[96];

    if (line_too_long) {
        uart2_send_text("error: line too long\r\n");
    } else {
        line[line_len] = '\0';                  /* el contrato de la cadena C */
        if (cmd_parse(line, &cmd)) {            /* false para una línea vacía */
            commands_execute(&cmd, reply, sizeof reply);
            uart2_send_text(reply);
            uart2_send_text("\r\n");
        }
    }
    line_len = 0U;
    line_too_long = false;
}



/* INTERRUPCIÓN*/

/* Manejador de interrupción de USART2: su nombre debe coincidir con la
   tabla de vectores. Solo pasa el evento al HAL, que averigua qué ocurrió. */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

/* Llamada por el HAL, dentro de la interrupción, cuando llegue el byte pedido.
   Escribimos nuestra propia versión porque HAL define __weak (un marcador vacío)
   Guarda el byte en el ring y pide el siguiente. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        rx_ring_put(rx_byte);
        uart2_start_rx();                   // preguntar a HAL por el siguiente byte
    }   
}

/* Llamada por el HAL, dentro de la interrupción, cuando USART2 reporta un error.
   Registra el error y reinicia la recepción. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        rx_last_error = HAL_UART_GetError(huart);
        rx_error_count++;
        uart2_start_rx();           //Los errores detienen la recepción. 
                                    //Cuando llega un byte antes de que se haya leído el anterior, 
                                    //el USART activa la bandera de desbordamiento (overrun).
    }
}

/* FUNCIONES PRIVADAS*/

/* Configura USART2 a 115200 8N1 en PA2/PA3 y le permite interrumpir a la CPU.
   Los pasos 1 a 3 son iguales que en la Etapa 1; el paso 4 es nuevo. */
static void uart2_init(void)
{
    /* pasos 1 a 3: exactamente como en la Etapa 1 (relojes, pines, periférico) */

    GPIO_InitTypeDef gpio = {0};

    /* 1. Relojes: USART2 (APB1) y GPIOA*/
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 2. PINES: PA2 = TX, PA3 = RX, función alternativa AF7 para ambos*/
    gpio.Pin                = GPIO_PIN_2 | GPIO_PIN_3;  //PA2/PA3
    gpio.Mode               = GPIO_MODE_AF_PP;          //FUNCIÓN ALTERNATIVA
    gpio.Pull               = GPIO_PULLUP;              //DESCANSA EN ALTO
    gpio.Speed              = GPIO_SPEED_FREQ_HIGH;     //HIGH SPEED
    gpio.Alternate          = GPIO_AF7_USART2;          //AF7
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

    /* 4. NVIC: aceptar peticiones de USART2, con prioridad mayor que la del heartbeat */
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* Le pide al HAL recibir un byte por interrupción dentro rx_byte.
   Retorna de inmediato; el byte llega después, a través de HAL_UART_RxCpltCallback(). */
static void uart2_start_rx(void)
{
    /* HAL_BUSY es normal tras un error de trama o de ruido: la recepción sigue activa */
    (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1U);
}

/* Envía una cadena C y espera hasta que haya salido (bloqueante, como en la Etapa 1). */
static void uart2_send_text(const char *text)
{
    (void)HAL_UART_Transmit(&huart2, (const uint8_t *)text,
                            (uint16_t)strlen(text), TX_TIMEOUT_MS);
}

/* Ring buffer, lado productor: se llama solo desde la interrupción.
   Guarda byte_to_put en rx_head. Si el ring está lleno, el byte se descarta. */
static void rx_ring_put(uint8_t byte_to_put)
{
    uint16_t next = (uint16_t)((rx_head + 1U) & (RX_RING_SIZE - 1U));

    if (next != rx_tail) {
        rx_ring[rx_head] = byte_to_put;
        rx_head = next;
    }
}

/* Ring buffer, lado consumidor: se llama solo desde app_loop().
   Copia el byte más antiguo en *byte_to_get. Retorna false si el ring está vacío. */
static bool rx_ring_get(uint8_t *byte_to_get)
{
    if (rx_tail == rx_head) {
        return false;
    }
    *byte_to_get = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) & (RX_RING_SIZE - 1U));
    return true;
}

