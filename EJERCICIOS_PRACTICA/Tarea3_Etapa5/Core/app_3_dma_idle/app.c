/* Core/app_3_dma_idle/app.c — Etapa 3: recepción por DMA, terminada por la línea IDLE */#include <stdbool.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "app.h"
#include "system.h"


#define TX_TIMEOUT_MS  100U
#define RX_DMA_SIZE     32U         //pequeño para llenar fácilmente 

static UART_HandleTypeDef huart2;
static DMA_HandleTypeDef hdma_usart2_rx;      //Dirección que se entrega a HAL debe ser variable static
static uint8_t rx_dma_buf[RX_DMA_SIZE];       //DMA esacribe los bytes recibidos
static volatile bool rx_event_ready = false;  //Bandera del Callback
static volatile uint16_t rx_event_size = 0U;
static volatile uint32_t rx_event_type = 0U;
static volatile uint32_t rx_error_count = 0U;
static volatile uint32_t rx_last_error = 0U;

static void uart2_init(void);
static void uart2_start_rx(void);
static void uart2_send_text(const char *text);
static void print_block(uint32_t event_type, uint16_t size);
static void event_name(uint32_t event_type, char *name, size_t name_size);



/* Corre una vez al arrancar, después del reloj y el heartbeat.
   Configura USART2, imprime el banner e inicia la primera recepción. */
void app_init(void)
{
    uart2_init();
    uart2_send_text("\r\nStage 3: DMA reception, ended by IDLE.\r\n");
    uart2_start_rx();
}

/* Se ejecuta en cada ciclo del while loop desde main(). Se toma cada byte 
   almacenado en la interrupción del ring y lo imprime; luego reporta cualquier
   error de recepción. */
void app_loop(void)
{
    static uint32_t reported_errors = 0U;
    char msg[64];

    if (rx_event_ready) {
        uint16_t size_aux;
        uint32_t type_aux;

        __disable_irq();                        //interrupciones desactivadas
        size_aux = rx_event_size;
        type_aux = rx_event_type;
        rx_event_ready = false;
        __enable_irq();

        //HAL_Delay(2000U);          /* RÓMPELO: eliminar después */

        print_block(type_aux, size_aux);
    }

    if (rx_error_count != reported_errors) {
        reported_errors = rx_error_count;
        (void)snprintf(msg, sizeof msg, "\r\n[uart error 0x%02lX, reception restarted]\r\n",
                       (unsigned long)rx_last_error);
        uart2_send_text(msg);
    }
    
}

/*INTERRUPCIÓN*/
//* Manejador de interrupción de USART2: La bandera IDLE y los errores de recepción pertenecen a USART2
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

//* Manejador de interrupción del stream 5 del DMA1: 
// "Búfer lleno" y los errores de DMA pertenecen al flujo DMA
//  Sin él, el HAL nunca se entera de que el DMA terminó.
void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_rx);
}

//reúne todo el hardware en torno a USART2 en una sola función, llamada por la HAL.
//El hardware alrededor de USART2 tiene ahora cuatro partes: 
// relojes, pines, un flujo DMA y dos interrupciones, y el orden importa.
void HAL_UART_MspInit(UART_HandleTypeDef *huart)    
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance != USART2) {
        return;
    }

    /* 1. Señal de reloj: USART2, GPIOA y DMA1 */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* 2. PA2 = TX, PA3 = RX, ambos AF7 */
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 3. USART2_RX es DMA1, flujo 5, canal 4 */
    hdma_usart2_rx.Instance                 = DMA1_Stream5;             //Stream 5
    hdma_usart2_rx.Init.Channel             = DMA_CHANNEL_4;            //Canal 4 (USART2_RX)
    hdma_usart2_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;     //Dirección: periférico->memoria
    hdma_usart2_rx.Init.PeriphInc           = DMA_PINC_DISABLE;         //Dirección fija
    hdma_usart2_rx.Init.MemInc              = DMA_MINC_ENABLE;          //Avanza la dirección de RAM (posiciones consecutivas del buffer)
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;      //lee unidades de un byte del periférico
    hdma_usart2_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;      //escribre unidades de un byte en memoria
    hdma_usart2_rx.Init.Mode                = DMA_NORMAL;               //se detiene si se llena (del mismo tamaño del buffer)
    hdma_usart2_rx.Init.Priority            = DMA_PRIORITY_HIGH;        //Alta prioridad frente a otros stream DMA
    hdma_usart2_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;     //Desactiva FIFO (Memoria temporal interna del DMA.First In, First Out: el primer dato que entra es el primero que sale)
    if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(huart, hdmarx, hdma_usart2_rx);   //conecta manejador UART con DMA

    /* 4. NVIC: la interrupción del DMA (TC) y la de USART2 (IDLE, errores) */
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 2, 0);  //Configura la prioridad de la interrupción del flujo DMA1 Stream 5
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);      //Habilita en el NVIC la IRQ de DMA1 Stream 5
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);        //Configura la prioridad de la interrupción de USART2
    HAL_NVIC_EnableIRQ(USART2_IRQn);            //Habilita en el NVIC la IRQ de USART2
}



/* Tres eventos, un callback. La HAL informa del fin de un bloque mediante 
HAL_UARTEx_RxEventCallback(huart, Size), donde Size es el número de bytes 
que hay ahora en el búfer. Tres eventos diferentes lo llaman, 
y HAL_UARTEx_GetRxEventType() indica cuál:*/
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2) {
        rx_event_size = Size;                               //Cuántos bytes se recibieron en el buffer
        rx_event_type = HAL_UARTEx_GetRxEventType(huart);   //Cuál es el evento  (IDLE o TC)
        rx_event_ready = true;                              //Bloque recibido por procesar
        uart2_start_rx();                                   //Reiniciar la recepción pausada por el evento
    }
}

/* Llamada por el HAL, dentro de la interrupción, cuando USART2 reporta un error.
   Registra el error y reinicia la recepción. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        rx_last_error = HAL_UART_GetError(huart);
        rx_error_count++;
        uart2_start_rx();
    }
}


/* FUNCIONES PRIVADAS*/

/* Configura USART2 a 115200 8N1 sin paridad, sin control de flujo y con 16 lecturas por baudio*/
static void uart2_init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200U;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

/*Inicia una recepción por DMA hacia rx_dma_buf que termina en una línea IDLE o cuando el
   buffer se llena. El HAL también avisa en la mitad de la transferencia; lo desactivamos. */

static void uart2_start_rx(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_dma_buf, RX_DMA_SIZE) == HAL_OK) {
        __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
    }
}

/* Envía una cadena C y espera hasta que haya salido (bloqueante, como en la Etapa 1). */
static void uart2_send_text(const char *text)
{
    (void)HAL_UART_Transmit(&huart2, (const uint8_t *)text,
                            (uint16_t)strlen(text), TX_TIMEOUT_MS);
}

/* Imprime un bloque recibido como [NOMBRE_EVENTO bytes] 'texto', leyendo rx_dma_buf
   directamente. Los bytes que no se pueden mostrar, como \r, aparecen como '.'. */
static void print_block(uint32_t event_type, uint16_t size)
{
    char name[5];                           //tamaño para IDLE + '\0'
    char text[RX_DMA_SIZE + 1U];
    char msg[RX_DMA_SIZE + 32U];

    if (size > RX_DMA_SIZE) {
        size = RX_DMA_SIZE;                 //Limite superior del buffer
    }
    for (uint16_t i = 0U; i < size; i++) {
        uint8_t byte_aux = rx_dma_buf[i];
        text[i] = ((byte_aux >= 0x20U) && (byte_aux < 0x7FU)) ? (char)byte_aux : '.';
    }
    text[size] = '\0';
    event_name(event_type, name, sizeof name);
    (void)snprintf(msg, sizeof msg, "[%s %2u bytes] '%s'\r\n",
                   name, (unsigned int)size, text);
    uart2_send_text(msg);
}

/* Escribe el nombre de un evento de HAL_UARTEx_GetRxEventType() en name[],
   un arreglo que pertenece al llamador (el mismo patrón que rx_ring_get()). */
static void event_name(uint32_t event_type, char *name, size_t name_size)
{
    switch (event_type) {
    case HAL_UART_RXEVENT_IDLE:
        (void)snprintf(name, name_size, "IDLE");
        break;
    case HAL_UART_RXEVENT_TC:
        (void)snprintf(name, name_size, "TC");
        break;
    case HAL_UART_RXEVENT_HT:
        (void)snprintf(name, name_size, "HT");
        break;
    default:
        (void)snprintf(name, name_size, "?");
        break;
    }
}