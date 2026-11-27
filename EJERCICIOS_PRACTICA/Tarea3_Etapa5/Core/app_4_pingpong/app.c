/* core/app_4_pingpong/app.c — aplicación final: UART, DMA ping-pong, comandos y PWM */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "app.h"
#include "system.h"
#include "cmd_parser.h"
#include "commands.h"
#include "pwm.h"


#define RX_BLOCK_SIZE  32U      // cada bloque puede guardar 32 bytes
#define TX_BUF_SIZE    256U     // espacio máximo para juntar texto 
#define LINE_MAX_LEN   63U      // máximo de caracteres del comando, sin contar el '\0'

/* aquí se reconstruye una línea aunque llegue repartida en varios bloques DMA */
static char line[LINE_MAX_LEN + 1U];      // el espacio extra guarda el terminador de cadena '\0'
static uint16_t line_len = 0U;            // cantidad de caracteres guardados
static bool line_too_long = false;        // = true cuando excede el límite

/* un bloque conserva los bytes recibidos hasta que app_loop() los procese */
typedef struct {
    uint8_t data[RX_BLOCK_SIZE];      // DMA deposita aquí los bytes recibidos
    volatile uint16_t size;           // número de bytes válidos dentro de data
    volatile bool ready;              // = true -> app_loop() debe procesarlo
} rx_block_t;

static rx_block_t rx_block[2];              // dos bloques alternos: A y B
static volatile uint8_t rx_dma_index = 0U;  // índice del bloque que está llenando el DMA
static volatile bool rx_paused = false;     // = true cuando A y B están ocupados al tiempo
static volatile uint32_t rx_pause_count = 0U; // cuenta las pausas de recepción

/* tx_fill recibe texto del programa; tx_dma_buf queda reservado al DMA mientras transmite */
static DMA_HandleTypeDef hdma_usart2_tx;      // static: el HAL guarda un puntero a él
static uint8_t tx_fill[TX_BUF_SIZE];          // app_loop() agrega texto aquí
static uint16_t tx_fill_len = 0U;             // bytes ocupados en tx_fill
static uint8_t tx_dma_buf[TX_BUF_SIZE];       // copia estable que el DMA lee durante el envío
static volatile bool tx_busy = false;         // ocupado: cambia a true mientras DMA está enviando


static UART_HandleTypeDef huart2;             // HAL usa este handle para configurar y operar USART2
static DMA_HandleTypeDef hdma_usart2_rx;      // Dirección que se entrega a HAL debe ser variable static
static volatile uint32_t rx_error_count = 0U; // contador de errores recibidos
static volatile uint32_t rx_last_error = 0U;  // último código de error de HAL

/* funciones que convierten bytes recibidos en líneas y comandos completos */
static void line_add_byte(uint8_t new_byte);
static void line_execute(void);

/* funciones privadas para inicializar UART/DMA y enviar respuestas sin bloquear */
static void uart2_init(void);
static void uart2_start_rx(void);
static void uart2_send_text(const char *text);

static void uart2_send_bytes(const uint8_t *data, uint16_t length);
static void uart2_flush(void);



/*
corre una sola vez al arrancar, después del reloj y el heartbeat
prepara PWM y UART, muestra el prompt y deja listo el primer bloque de recepción
*/
void app_init(void)
{
    uart2_init();                                  // configura USART2 y DMA
    pwm_init();                                    // arranca TIM3 en PC6 
    uart2_send_text("\r\nStage 4: ping-pong reception.\r\n> "); // banner
    uart2_start_rx();                             // recibe en el bloque indicado por rx_dma_index
}

/*
se ejecuta en cada ciclo del while loop desde main(). Usa el siguiente bloque
cuando el DMA ya lo terminó, lo devuelve, y reinicia la recepción si estaba
en pausa
*/
void app_loop(void)
{
    static uint8_t read_index = 0U;               // siguiente bloque: A o B
    static uint32_t reported_errors = 0U;         // último total de errores informados
    static uint32_t reported_pauses = 0U;         // último total de pausas informadas
    char msg[64];                                 // texto temporal de errores UART

    /* 1. usar el siguiente bloque, si el DMA ya lo terminó */
    if (rx_block[read_index].ready) {
        bool restart_aux;                         // si el DMA quedó pausado por falta de bloques

        /* el bloque ya está listo; añadir sus bytes a la línea, uno por uno */
        for (uint16_t i = 0U; i < rx_block[read_index].size; i++)
        {
            line_add_byte(rx_block[read_index].data[i]);
        }

        /* actualizar ready y rx_paused sin permitir que el callback interrumpa a mitad */
        __disable_irq();
        rx_block[read_index].ready = false;       // ready = false -> disponible para DMA
        restart_aux = rx_paused;
        rx_paused = false;                      

        if (restart_aux) {
            rx_dma_index = read_index;            // el DMA esperaba que este bloque quedara libre
            uart2_start_rx();
        }
        read_index = (uint8_t)(1U - read_index);  // alterna: 0 pasa a 1 y 1 pasa a 0
    }

    /* 2. mostrar cada error nuevo que haya registrado la interrupción de UART */
    if (rx_error_count != reported_errors) {
        reported_errors = rx_error_count;
        (void)snprintf(msg, sizeof msg, "\r\n[uart error 0x%02lX, reception restarted]\r\n",
                       (unsigned long)rx_last_error);
        uart2_send_text(msg);
    }
    /* informar cuando ambos bloques estuvieron ocupados y se perdieron bytes */
    if (rx_pause_count != reported_pauses) {
        reported_pauses = rx_pause_count;
        uart2_send_text("\r\n[reception paused: both blocks busy, data lost]\r\n");
    }

    /* 3. si TX DMA está libre, comenzar a transmitir lo que se acumuló en tx_fill */
    uart2_flush();
}

/* la interrupción de USART2 avisa sobre IDLE y errores de recepción */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

/* el flujo 5 avisa cuando termina o falla una recepción por DMA */
void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_rx);
}

/* el flujo 6 avisa cuando termina o falla una transmisión por DMA */
void DMA1_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_tx);
}

/* la HAL llama esta función para configurar los recursos usados por USART2 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)    
{
    GPIO_InitTypeDef gpio = {0};

    /* este módulo configura recursos solo para USART2; ignora otros UART si los hubiera */
    if (huart->Instance != USART2) {
        return;
    }

    /* 1. Señal de reloj: USART2, GPIOA y DMA1 */
     __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* 2. conectar PA2/PA3 con USART2 usando la función alternativa AF7 */
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 3a. preparar el DMA de recepción: USART2_RX está conectado al flujo 5, canal 4 */
    hdma_usart2_rx.Instance                 = DMA1_Stream5;         // stream5
    hdma_usart2_rx.Init.Channel             = DMA_CHANNEL_4;        // canal 4 USART2_RX
    hdma_usart2_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY; // dirección: periférico -> memoria
    hdma_usart2_rx.Init.PeriphInc           = DMA_PINC_DISABLE;     // dirección fija
    hdma_usart2_rx.Init.MemInc              = DMA_MINC_ENABLE;      // avanza por las posiciones del buffer
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;  // lee unidades de un byte del periférico
    hdma_usart2_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;  // escribre unidades de un byte en memoria
    hdma_usart2_rx.Init.Mode                = DMA_NORMAL;           // se detiene al completar el bloque
    hdma_usart2_rx.Init.Priority            = DMA_PRIORITY_HIGH;    // prioridad alta entre flujos DMA
    hdma_usart2_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE; //Desactiva FIFO (Memoria temporal interna del DMA.First In, First Out: el primer dato que entra es el primero que sale)
    if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(huart, hdmarx, hdma_usart2_rx);   // asocia el DMA de recepción con el handle USART

    /* 3b. preparar el DMA de transmisión: USART2_TX está conectado al flujo 6, canal 4 */
    hdma_usart2_tx.Instance                 = DMA1_Stream6;         // stream6
    hdma_usart2_tx.Init.Channel             = DMA_CHANNEL_4;        // canal 4 (USART_TX)
    hdma_usart2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH; // dirección: memoria -> periférico
    hdma_usart2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;     // dirección fija
    hdma_usart2_tx.Init.MemInc              = DMA_MINC_ENABLE;      // avanza por las posiciones del buffer
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;  // lee unidades de un byte del periférico
    hdma_usart2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;  // escribre unidades de un byte en memoria
    hdma_usart2_tx.Init.Mode                = DMA_NORMAL;           // se detiene al completar el bloque
    hdma_usart2_tx.Init.Priority            = DMA_PRIORITY_LOW;     // prioridad baja frente a otros flujos DMA
    hdma_usart2_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE; //Desactiva FIFO (Memoria temporal interna del DMA.First In, First Out: el primer dato que entra es el primero que sale)
    if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(huart, hdmatx, hdma_usart2_tx);

    /* 4. dar prioridad y habilitar las IRQ de RX DMA, USART2 (IDLE/errores) y TX DMA */
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}



/* HAL llama este callback cuando ocurre un evento de recepción por DMA */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    /* hT se desactiva al iniciar RX; se atienden los eventos IDLE y bloque completo (TC) */
    if ((huart->Instance == USART2) &&
        (HAL_UARTEx_GetRxEventType(huart) != HAL_UART_RXEVENT_HT)) {
        uint8_t filled_index = rx_dma_index;                // bloque que acaba de llenar  DMA
        uint8_t other_index = (uint8_t)(1U - filled_index); // el otro bloque podría estar libre

        rx_block[filled_index].size = Size;                 // registrar cuántos bytes válidos llegaron
        rx_block[filled_index].ready = true;                // ready = true -> bloque de app_loop()
        if (!rx_block[other_index].ready) {
            rx_dma_index = other_index;                     // cambiar al otro bloque y seguir recibiendo
            uart2_start_rx();
        } else {
            rx_paused = true;                               // no hay dónde recibir hasta liberar un bloque
            rx_pause_count++;
        }
    }
}

/* HAL llama este callback cuando USART2 reporta un error de comunicación */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        rx_last_error = HAL_UART_GetError(huart);   // guardar la causa para mostrarla desde app_loop()
        rx_error_count++;                           // señalar que ocurrió un error nuevo
        if (huart->gState == HAL_UART_STATE_READY) 
        {
            tx_busy = false;                        // error de transmisión
        }
        if (!rx_paused) {
            uart2_start_rx();                       // reiniciar RX si el sistema no la pausó por falta de bloques
        }
    }
}

/* la HAL llama esta función cuando el DMA terminó de enviar el bloque */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        tx_busy = false;                    // ya terminó; uart2_flush() podrá iniciar el siguiente envío
    }
}


/* fUNCIONES PRIVADAS */

/* configura USART2 a 115200 baudios, formato 8N1, sin control de flujo y con sobremuestreo por 16 */
static void uart2_init(void)
{
    huart2.Instance          = USART2;                 // USART2
    huart2.Init.BaudRate     = 115200U;                // 115200 baudios 
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;     // 8its
    huart2.Init.StopBits     = UART_STOPBITS_1;        // 1 bit de parada
    huart2.Init.Parity       = UART_PARITY_NONE;       // sin paridad
    huart2.Init.Mode         = UART_MODE_TX_RX;        // transmisión y recepción
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;    // sin RTS/CTS
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;   // sobremuestreo por 16
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

/*
empieza a recibir en el bloque elegido; termina por IDLE o cuando el bloque se llena
desactiva HT porque esta aplicación procesa bloques completos, no medias transferencias
*/
static void uart2_start_rx(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_block[rx_dma_index].data,
                                     RX_BLOCK_SIZE) == HAL_OK) {
        __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
    }
}

/* agrega datos a la cola TX sin esperar; si no hay espacio, descarta el bloque entero */
static void uart2_send_bytes(const uint8_t *data, uint16_t length)
{
    /* evitar copiar más bytes que el espacio libre que queda en tx_fill */
    if (length > (TX_BUF_SIZE - tx_fill_len)) {
        return;
    }
    /* copiar al final de los datos pendientes y actualizar el total ocupado */
    memcpy(&tx_fill[tx_fill_len], data, length);
    tx_fill_len = (uint16_t)(tx_fill_len + length);
}

/* convierte la cadena C a bytes y los deja en cola para transmisión por DMA */
static void uart2_send_text(const char *text)
{
    uart2_send_bytes((const uint8_t *)text, (uint16_t)strlen(text));
}

/* inicia TX si no hay un envío activo y existen bytes en la cola */
static void uart2_flush(void)
{
    if ((!tx_busy) && (tx_fill_len > 0U)) {
        memcpy(tx_dma_buf, tx_fill, tx_fill_len);   // el DMA usa una copia que no cambia durante el envío
        tx_busy = true;                             // marcar ocupado antes de pedirle el envío a HAL
        if (HAL_UART_Transmit_DMA(&huart2, tx_dma_buf, tx_fill_len) == HAL_OK) {
            tx_fill_len = 0U;                       // se limpia para nuevos mensajes
        } else {
            tx_busy = false;                        // si HAL no lo inició, se volverá a intentar en otro ciclo
        }
    }
}



/*
agrega un byte recibido a la línea que se está escribiendo, y le hace eco
enter (\r, \n o \r\n) termina la línea y ejecuta line_execute()
*/
static void line_add_byte(uint8_t new_byte)
{
    static uint8_t previous_byte = 0U; // recuerda el byte anterior para reconocer el final CRLF

    /* si CR ya cerró la línea, ignorar el LF que forma la segunda mitad de CRLF */
    if ((new_byte == '\n') && (previous_byte == '\r')) {
        previous_byte = new_byte;
        return;                                 // segunda mitad de "\r\n"
    }
    previous_byte = new_byte;                   // guardar este byte para comparar con el siguiente

    if ((new_byte == '\r') || (new_byte == '\n')) {     // CR o LF indica que terminó el comando
        uart2_send_text("\r\n");                // avanzar a una línea en la terminal
        line_execute();                             // analizar y ejecutar el texto acumulado
        uart2_send_text("> ");                  // mostrar el prompt para el siguiente comando
    } else if (line_len < LINE_MAX_LEN) {
        line[line_len] = (char)new_byte;         // guardar el carácter al final de la línea actual
        line_len++;                              // actualizar la cantidad de caracteres acumulados
        uart2_send_bytes(&new_byte, 1U);        /* hacerle eco: se agrega al texto por enviar */
    } else {
        line_too_long = true;                   // seguir consumiendo bytes, pero rechazar el comando completo
    }
}

/*
convierte la línea terminada en una cadena C, la analiza y ejecuta el comando,
y luego vacía la línea para la siguiente. las líneas vacías se ignoran
*/
static void line_execute(void)
{
    cmd_t cmd;                                  // aquí cmd_parse deja el nombre y sus argumentos
    char reply[96];                             // espacio para la respuesta del comando

    /* no ejecutar una línea que superó el máximo permitido */
    if (line_too_long) {
        uart2_send_text("error: line too long\r\n");
    } else {
        line[line_len] = '\0';                  // convertir los bytes acumulados en una cadena C
        if (cmd_parse(line, &cmd)) {            // false significa que el usuario solo envió enter
            commands_execute(&cmd, reply, sizeof reply); // ejecutar el comando y llenar reply
            uart2_send_text(reply);             // poner la respuesta en la cola TX
            uart2_send_text("\r\n");            // terminar la respuesta con una nueva línea
        }
    }
    line_len = 0U;                              // preparar el buffer para el próximo comando
    line_too_long = false;                      // quitar la marca de exceso para la nueva línea
}
