/* Core/app_4_pingpong/app.c — Etapa 4: recepción ping-pong, transmisión DMA y comandos */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "app.h"
#include "system.h"
#include "cmd_parser.h"
#include "commands.h"


#define RX_BLOCK_SIZE  32U      // pequeño a propósito: las ráfagas largas igual se dividen en dos
#define TX_BUF_SIZE    256U
#define LINE_MAX_LEN   63U      //Linea de comando más larga sin '\0'

static char line[LINE_MAX_LEN + 1U];
static uint16_t line_len = 0U;
static bool line_too_long = false;

typedef struct {
    uint8_t data[RX_BLOCK_SIZE];            //para DMA escribir los bytes recibidos
    volatile uint16_t size;                 // bytes recibidos (ready = true)
    volatile bool ready;                    // true: el bloque le pertenece a app_loop()
} rx_block_t;

static rx_block_t rx_block[2];                  //Bloques A y B
static volatile uint8_t rx_dma_index = 0U;      //Bloque que llena DMA
static volatile bool rx_paused = false;         //Cuando ambos bloques estén ocupados
static volatile uint32_t rx_pause_count = 0U;

static DMA_HandleTypeDef hdma_usart2_tx;        // static: el HAL guarda un puntero a él
static uint8_t tx_fill[TX_BUF_SIZE];            //app_loop() agrega texto aquí
static uint16_t tx_fill_len = 0U;
static uint8_t tx_dma_buf[TX_BUF_SIZE];         //DMA lee de aca mientras envía
static volatile bool tx_busy = false;           //cambia a true mientras DMA está enviando


static UART_HandleTypeDef huart2;
static DMA_HandleTypeDef hdma_usart2_rx;      //Dirección que se entrega a HAL debe ser variable static
static volatile uint32_t rx_error_count = 0U;
static volatile uint32_t rx_last_error = 0U;

static void line_add_byte(uint8_t new_byte);
static void line_execute(void);

static void uart2_init(void);
static void uart2_start_rx(void);
static void uart2_send_text(const char *text);

static void uart2_send_bytes(const uint8_t *data, uint16_t length);
static void uart2_flush(void);



/* Corre una vez al arrancar, después del reloj y el heartbeat.
   Configura USART2, imprime el banner e inicia la primera recepción. */
void app_init(void)
{
    uart2_init();
    uart2_send_text("\r\nStage 4: ping-pong reception.\r\n> ");
    uart2_start_rx();
}

/* Se ejecuta en cada ciclo del while loop desde main(). Usa el siguiente bloque 
    cuando el DMA ya lo terminó, lo devuelve, y reinicia la recepción si estaba 
    en pausa. */
void app_loop(void)
{
    static uint8_t read_index = 0U;
    static uint32_t reported_errors = 0U;
    static uint32_t reported_pauses = 0U;
    char msg[64];

    /* 1. Usar el siguiente bloque, si el DMA ya lo terminó */
    if (rx_block[read_index].ready) {
        bool restart_aux;


        for (uint16_t i = 0U; i < rx_block[read_index].size; i++)
        {
            line_add_byte(rx_block[read_index].data[i]);
        }

        __disable_irq();                            //Devolver el bloque
        rx_block[read_index].ready = false;
        restart_aux = rx_paused;
        rx_paused = false;
        __enable_irq();

        if (restart_aux) {
            rx_dma_index = read_index;              //La recepción esperaba este bloque
            uart2_start_rx();
        }
        read_index = (uint8_t)(1U - read_index);    // A -> B -> A -> B
    }

    /* 2. Reportar lo que vieron las interrupciones */
    if (rx_error_count != reported_errors) {
        reported_errors = rx_error_count;
        (void)snprintf(msg, sizeof msg, "\r\n[uart error 0x%02lX, reception restarted]\r\n",
                       (unsigned long)rx_last_error);
        uart2_send_text(msg);
    }
    if (rx_pause_count != reported_pauses) {
        reported_pauses = rx_pause_count;
        uart2_send_text("\r\n[reception paused: both blocks busy, data lost]\r\n");
    }

    /* 3. Empezar a enviar el texto que está esperando */
    uart2_flush();
}

/* La interrupción de USART2 avisa sobre IDLE y errores de recepción. */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

/* El flujo 5 avisa cuando termina o falla una recepción por DMA. */
void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_rx);
}

/* El flujo 6 avisa cuando termina o falla una transmisión por DMA. */
void DMA1_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_tx);
}

/* La HAL llama esta función para configurar los recursos usados por USART2. */
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

    /* 3a. DMA para RX: USART2_RX usa el flujo 5, canal 4. */
    hdma_usart2_rx.Instance                 = DMA1_Stream5;         //Flujo usado para recibir
    hdma_usart2_rx.Init.Channel             = DMA_CHANNEL_4;        //Canal conectado a USART2_RX
    hdma_usart2_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY; //Guarda en RAM lo que recibe USART2
    hdma_usart2_rx.Init.PeriphInc           = DMA_PINC_DISABLE;     //Lee siempre el mismo registro USART
    hdma_usart2_rx.Init.MemInc              = DMA_MINC_ENABLE;      //Avanza por las posiciones del buffer
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;  //Lee un byte por transferencia
    hdma_usart2_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;  //Guarda un byte por transferencia
    hdma_usart2_rx.Init.Mode                = DMA_NORMAL;           //Se detiene al completar el bloque
    hdma_usart2_rx.Init.Priority            = DMA_PRIORITY_HIGH;    //Prioridad alta entre flujos DMA
    hdma_usart2_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE; //Transfiere directamente, sin FIFO
    if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(huart, hdmarx, hdma_usart2_rx);   //Asocia el DMA de recepción con el handle USART

    /* 3b. DMA para TX: USART2_TX usa el flujo 6, canal 4. */
    hdma_usart2_tx.Instance                 = DMA1_Stream6;         //Stream6
    hdma_usart2_tx.Init.Channel             = DMA_CHANNEL_4;        //Canal 4 (USART_TX)
    hdma_usart2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH; //Envía datos desde RAM hacia USART2
    hdma_usart2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;     //Dirección fija
    hdma_usart2_tx.Init.MemInc              = DMA_MINC_ENABLE;      //Avanza a la siguiente posición de tx_dma_buf
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;  //Envía datos de un byte a USART2
    hdma_usart2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;  //Lee datos de un byte desde RAM
    hdma_usart2_tx.Init.Mode                = DMA_NORMAL;           //Se detiene al enviar la cantidad programada
    hdma_usart2_tx.Init.Priority            = DMA_PRIORITY_LOW;     //Prioridad baja frente a otros flujos DMA
    hdma_usart2_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE; //Envía directamente, sin usar la FIFO interna
    if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(huart, hdmatx, hdma_usart2_tx);

    /* Activa las interrupciones de recepción, USART2 y transmisión. */
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}



/* Llamada por el HAL, dentro de una interrupción, cuando llegó un bloque (IDLE o TC).*/
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if ((huart->Instance == USART2) &&
        (HAL_UARTEx_GetRxEventType(huart) != HAL_UART_RXEVENT_HT)) {
        uint8_t filled_index = rx_dma_index;
        uint8_t other_index = (uint8_t)(1U - filled_index);

        rx_block[filled_index].size = Size;
        rx_block[filled_index].ready = true;     //ready = true -> Bloque de app_loop()
        if (!rx_block[other_index].ready) {
            rx_dma_index = other_index;          //Bloque DMA libre
            uart2_start_rx();
        } else {
            rx_paused = true;                    //pausa: los dos bloques están ocupados
            rx_pause_count++;
        }
    }
}

/* Llamada por el HAL, dentro de la interrupción, cuando USART2 reporta un error.
   Registra el error y reinicia la recepción. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        rx_last_error = HAL_UART_GetError(huart);
        rx_error_count++;
        if (huart->gState == HAL_UART_STATE_READY) 
        {
            tx_busy = false;                     //error de transmisión
        }
        if (!rx_paused) {
            uart2_start_rx();                    //cada error detiene la recepción
        }
    }
}

/* La HAL llama esta función cuando el DMA terminó de enviar el bloque. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        tx_busy = false;                    /* permite que uart2_flush() inicie otro envío */
    }
}


/* FUNCIONES PRIVADAS*/

/* Configura USART2 a 115200 baudios, formato 8N1, sin control de flujo y con sobremuestreo por 16. */
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

/* Inicia una recepción por DMA hacia el bloque rx_dma_index, que termina en una línea
   IDLE o cuando el bloque se llena. El HAL también activa HT; lo desactivamos. */
static void uart2_start_rx(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_block[rx_dma_index].data,
                                     RX_BLOCK_SIZE) == HAL_OK) {
        __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
    }
}

/* Guarda los bytes para enviarlos después; si no caben, descarta este bloque completo. */
static void uart2_send_bytes(const uint8_t *data, uint16_t length)
{
    if (length > (TX_BUF_SIZE - tx_fill_len)) {
        return;
    }
    memcpy(&tx_fill[tx_fill_len], data, length);
    tx_fill_len = (uint16_t)(tx_fill_len + length);
}

/* Agrega una cadena a la cola de envío sin esperar a que termine la transmisión. */
static void uart2_send_text(const char *text)
{
    uart2_send_bytes((const uint8_t *)text, (uint16_t)strlen(text));
}

/* Si el DMA está libre, copia lo pendiente a su buffer e inicia el envío. */
static void uart2_flush(void)
{
    if ((!tx_busy) && (tx_fill_len > 0U)) {
        memcpy(tx_dma_buf, tx_fill, tx_fill_len);
        tx_busy = true;                         /* ponerlo primero: el callback puede ser rápido */
        if (HAL_UART_Transmit_DMA(&huart2, tx_dma_buf, tx_fill_len) == HAL_OK) {
            tx_fill_len = 0U;
        } else {
            tx_busy = false;                    /* no se inició: reintentar la próxima vez */
        }
    }
}



/* Agrega un byte recibido a la línea que se está escribiendo, y le hace eco.
   Enter (\r, \n o \r\n) termina la línea y ejecuta line_execute(). */
static void line_add_byte(uint8_t new_byte)
{
    static uint8_t previous_byte = 0U;

    if ((new_byte == '\n') && (previous_byte == '\r')) {
        previous_byte = new_byte;
        return;                                 // segunda mitad de "\r\n"
    }
    previous_byte = new_byte;

    if ((new_byte == '\r') || (new_byte == '\n')) {    //Fin de la línea
        uart2_send_text("\r\n");
        line_execute();
        uart2_send_text("> ");
    } else if (line_len < LINE_MAX_LEN) {
        line[line_len] = (char)new_byte;
        line_len++;
        uart2_send_bytes(&new_byte, 1U);        /* hacerle eco: se agrega al texto por enviar */
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