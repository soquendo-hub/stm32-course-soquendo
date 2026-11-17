/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Steven A. Oquendo Rave
 * @brief          : Encoder KY040 (TIM5) ajusta individualmente CCR1, CCR3, CCR4
 *                   o los tres valores del PWM (TIM3); EXTI0 selecciona el modo,
 *                   organizado como una Máquina de Estados Finitos (FSM) 
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 *
 *** Mapeo de pines:
 ** PWM TIM3 - AF02:
 * - CH1 (PWM) -> PC6 (RED)
 * - CH3 (PWM) -> PC8 (GREEN)
 * - CH4 (PWM) -> PC9 (BLUE)
 * - Indicadores de modo: PC10 (RED), PC12 (GREEN), PC11 (BLUE)
 * - CH2 (PWM) -> PC7 (DAC)
 *
 ** ENCODER TIM5 - AF02:
 * - CH1 (KY040: CLK) -> PA0
 * - CH2 (KY040: DT)  -> PA1
 *
 ** PULSADOR KY040 (SW), activo en bajo:
 * - SW -> PB0 (EXTI0)
 *
 ** BLINKY TIM4 -> PH1 (4Hz en modo CCR, 2 Hz en modo DAC)
 *
 ** CAPTURA TIM2 - AF01
 * - CH3 (CAPTURE) -> PB10
 *
 *
 *====================== FUNCIONAMIENTO ===================== *
 *
 * -TIM5 en modo encoder cuenta los flancos de CLK y DT (4 cuentas por paso).
 *   Cada paso del encoder sube o baja en una cantidad fija el calor que se esté
 *   editando.
 *
 * -TIM3 genera PWM en PC6 (CH1), PC7 (CH2), PC8 (CH3) y PC9 (CH4). Con PSC = 15 cada tick
 *  dura 1 us: periodo = (ARR + 1) us. Arranca con ARR = 1999 y CCR1/3/4 = 1000:
 *  500 Hz con duty de 50% en las tres salidas.
 *
 * -Hay cinco modos de edición y cada flanco de bajada del pulsador avanza al siguiente:
 *  MODE_CCR1/2/3/4: cada paso suma o resta 10 al ancho del pulso seleccionado.
 *  MODE_RGB: cada paso suma o resta 10 a los tres anchos de pulso.
 *  Los CCR se limitan a 0..ARR + 1. Los LED indicadores muestran el modo activo.
 *
 * -TIM4 es la base de tiempo: interrumpe cada 1 ms y cuenta milisegundos. El LED PH1
 *  parpadea a 4 Hz.
 *
 * -TIM2 captura cada flanco de subida en CCR3. La resta entre capturas consecutivas
 *  deja el periodo en periodo_ticks (ticks de 1 us con el reloj TIM2 actual).
 *
 * -Reloj de TIM2: PSC = 15 supone un reloj de entrada de 16 MHz y genera ticks de 1 us.
 *
 *
 *===================== FSM (presentación 05-FSM) ===================== *
 *
 *  patron 1: Switch-Case. La FSM decide qué pasa (política), las funciones
 *  driver hacen el cómo (hardware). Las ISR solo levantas banderas volatile,
 *  get_next_event() las convierte en un evento y el while(1) nunca se bloquea.
 *
 *  Estado actual      Evento               Estado siguiente    Acción [ENTER]
 *   -----------------  -------------------  ------------------  --------------------------
 *   STATE_IDLE         EV_ENCODER_CHANGED   STATE_UPDATE_PWM    -
 *   STATE_IDLE         EV_BUTTON_PRESS      STATE_CHANGE_MODE   -
 *   STATE_IDLE         EV_BLINK_TICK        STATE_BLINK         -
 *   STATE_IDLE         EV_CAPTURE           STATE_UPDATE_CAPT   -
 *   STATE_UPDATE_PWM   (incondicional)      STATE_IDLE          pwm_ApplySteps(encoder_TakeSteps())
 *   STATE_CHANGE_MODE  (incondicional)      STATE_IDLE          mode_Toggle()
 *   STATE_BLINK        (incondicional)      STATE_IDLE          led_Toggle()
 *   STATE_UPDATE_CAPT  (incondicional)      STATE_IDLE          publica periodo_ticks
 *   STATE_ERROR        (cualquiera)         STATE_IDLE          recuperación
 ******************************************************************************
 */

/* Implementación HAL de Tarea2: USART2 reporta el modo y la última frecuencia medida sin bloquear. */

#include "stm32f4xx_hal.h"

#include <stdint.h>
#include <stdio.h>

// Constantes
#define COUNTS_PER_STEP       4       //El modo TI12 cuenta 4 flancos por cada paso mecánico del encoder
#define ARR_INIT              1999U   //ARR inicial: (1999 + 1) x 1 us = 2000 us -> 500 Hz
#define CCR_INIT              ((ARR_INIT + 1U) / 2U) //La mitad del periodo: 1000 us de 2000 us -> duty de 50 %
#define CCR_STEP              10      //Lo que suma o resta al CCR seleccionado en cada paso
#define BLINK_HALF_PERIOD_MS  125U    //PH1 cambia cada 125 ms: periodo de 250 ms -> 4 Hz
#define BUTTON_DEBOUNCE_MS    20U     //Tiempo estable requerido para validar la pulsación y la liberación
#define TIMER_TICK_HZ         1000000U //Frecuencia del contador TIM2 tras aplicar PSC = 15
#define MODE_REPORT_QUEUE_LEN 8U

/* ========= MÁQUINA DE ESTADOS FINITOS =========*/

//  Paso 1 (FSM): mapear el vocabulario a memoria. Estados y eventos son enum (type safety)
//  y NUM_STATES /NUM_EVENTS son centinelas para validar rangos.

//  MODOS DE EDICIÓN: qué valor del PWM modifica el encoder
typedef enum {      //Lista de modos posibles
    MODE_CCR1,      //El encoder cambia el ancho de pulso de PC6 / TIM3_CH1
    MODE_CCR3,      //El encoder cambia el ancho de pulso de PC8 / TIM3_CH3
    MODE_CCR4,      //El encoder cambia el ancho de pulso de PC9 / TIM3_CH4
    MODE_RGB,       //El encoder cambia por igual los tres anchos de pulso
    MODE_DAC,       //El encoder cambia el ancho de pulso de PC7 / TIM3_CH2
    NUM_MODES
} Mode_t;           //Nombre del tipo: edit_mode solo puede recibir valores de este enum

//ESATDOS: la condición en la que esatá el sistema en un instante dado
typedef enum {          //Lista de estados posibles, cada nombre vale un entero (0, 1, 2...)
    STATE_IDLE,         //Reposo: espera un evento sin hacer nada más (no bloquea)
    STATE_UPDATE_PWM,   //Atiende el giro: ajusta el canal CCR o el periodo seleccionado
    STATE_CHANGE_MODE,  //Atiende el pulsador: avanza entre los cuatro modos
    STATE_BLINK,        //Atiende el tick del TIM4: cambia el estado del LED PH1
    STATE_UPDATE_CAPT,  //Atiende una captura pendiente de la señal de entrada
    STATE_ERROR,        //Estado inválido (tranpa de hardware): se recupera hacia STATE_IDLE
    NUM_STATES          //Centinela: cantidad de estados, sirve para validar el rango
} State_t;              //Nombre del tipo: current_state solo puede recibir valores de este enum

//EVENTOS: los disparadores (ISR o lecturas ) que provocan transiciones entre estados
typedef enum {          //Lista de eventos posibles, cada nombre vale un entero (0, 1, 2...)
    EV_NONE,            //No ha ocurrido nada
    EV_ENCODER_CHANGED, //El encoder avanzó o retrocedió al menos un paso
    EV_BUTTON_PRESS,    //El pulsador del KY040 fue presionado (flanco de bajada)
    EV_BLINKY_TICK,     //Venció el medio periodo del blinky (125 ms)
    EV_CAPTURE,         //Hay un periodo capturado pendiente de procesar
    NUM_EVENTS          //Centinela: cantidad de estados, sirve para validar el rango
} Event_t;              //Nombre del ripo: current_event solo puede recibir valores de este enum

typedef struct {
    Mode_t mode;
    uint32_t frequency_hz;
} UartModeReport_t;

/* Handles HAL; sus IRQ se despachan desde stm32f4xx_it.c. */
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;
UART_HandleTypeDef huart2;

//Estado actual de la FSM, static: solo se accede desde este módulo (scope mínimo) y solo se modifica en main
static State_t current_state = STATE_IDLE; //Siempre iniicia en reposo
//Bandera de ENTRY: vale 1 al llegar a un estado para que su acción [ENTER] se ejecute exactamente una vez
static uint8_t state_entry_flag = 1U; //Inicia en 1 para que el primer estado también ejecute su [ENTER]
static Mode_t edit_mode = MODE_CCR1; //Modo inicial: editar PC6 / TIM3_CH1

volatile uint8_t button_flag = 0U; //Bandera que deja la ISR del pulsador (volatile: cambia fuera del flujo normal del programa)
volatile uint8_t blink_flag = 0U; //Bandera que deja la ISR del TIM4 (volatile por la misma razoń)
static volatile uint8_t capture_flag;
static volatile uint32_t capture_period_pending;
static uint32_t capture_previous;
static uint8_t capture_previous_valid;

static volatile uint8_t button_debounce_ms;
static volatile uint8_t button_wait_release;
static uint16_t blink_count; //static: conserva su valor entre interrupciones
static const uint16_t blink_half_period_ms = BLINK_HALF_PERIOD_MS; //Milisegundos entre cambios del LED PH1

volatile uint32_t periodo_ticks; //Último periodo procesado por la FSM, expresado en ticks (1us)
volatile uint32_t frecuencia_hz; //Frecuencia calculada en Hz (resolución de 1 Hz)

static int32_t arr_value = (int32_t)ARR_INIT; //ARR que tiene el TIM3 (periodo = ARR +  1 ticks de 1 us), con signo para poder comparar con los límites
static int32_t ccr1_value = (int32_t)CCR_INIT; //Ancho de pulso de PC6, inicia en 50%
static int32_t ccr2_value = (int32_t)CCR_INIT; //Ancho de pulso de PC7, inicia en 50%
static int32_t ccr3_value = (int32_t)CCR_INIT; //Ancho de pulso de PC8, inicia en 50%
static int32_t ccr4_value = (int32_t)CCR_INIT; //Ancho de pulso de PC9, inicia en 50%

/* USART2 informa cambios de modo y la última frecuencia, sin bloquear. */
static UartModeReport_t uart_mode_queue[MODE_REPORT_QUEUE_LEN];
static uint8_t uart_mode_queue_head;
static uint8_t uart_mode_queue_tail;
static uint8_t uart_mode_queue_count;
static char uart_tx_buffer[64];

/*  =========== Definición de funciones ===========*/

//Drivers de la FSM (el cómo)
static Event_t get_next_event(void);        //lee (y limpia) las banderas de la ISR y la lectura del encoder, y las convierte en un evento
static int32_t encoder_TakeSteps(void);     //Devuelve los paso acumulados (con signo) y los descuenta del TIM5
static void pwm_ApplySteps(int32_t steps);  //Ajusta el CCR seleccionado o los tres, con saturación
static void mode_Toggle(void);              //Avanza entre los cinco modos
static void mode_Indicators_Update(void);   //Actualiza los LED indicadores según el modo activo
static void led_Toggle(void);               //Cambia el estado del LED PH1

//Inicialización del hardware
static void gpio_Init(void);                //GPIOH PH1 y GPIOC PC10/PC11/PC12 como salidas
static void tim4_Init(void);                //TIM4 con interrupción cada 1 ms (base de tiempo del blinky)
static void tim3_pwm_Init(void);            //GPIOC PC6(TIM3_CH1), PC7(TIM3_CH2), PC8(TIM3_CH3) Y PC9(TIM3_CH4) en modo PWM 1
static void tim5_encoder_Init(void);        //GPIOA PA0, PA1 + TIM5 en modo encoder
static void tim2_capture_Init(void);        //GPIOB PB10 en modo captura
static void usart2_Init(void);              //USART2 en PA2/PA3 para reportes

static void uart_ReportService(void);
static void uart_QueueModeReport(void);
static int32_t pwm_ClampCompare(int32_t value);
static void Error_Handler(void);

/* ========== Main ========== */

int main(void) //Punto de entrada: el startup lo llama después de copiar .data y limpiar .bss
{
    if (HAL_Init() != HAL_OK) {
        Error_Handler();
    }

    SystemClock_Config(); //Configura HSI a 16 MHz y APB1 sin divisor
    gpio_Init(); //Configura PH1 (blinky)
    //Configura el pulsador por EXTI0 en PB0
    tim3_pwm_Init(); //Configura las tres salidas PWM de TIM3
    tim5_encoder_Init(); //Configura el encoder
    tim2_capture_Init(); //Configura la captura
    usart2_Init(); //Configura USART2 para reportes no bloqueantes
    tim4_Init(); //Último: las interrupciones del blinky empiezan con todo el hardware listo

    /* Patrón 1 (FSM): Switch-Case. El ciclo infinito nunca termina: lee un evento, evalúa el estado y reacciona.
	 * Regla de NO bloqueo: no hay delay(), cada pasada procesa a lo sumo UN evento y UNA transición. */
    while (1) //El firmware nunca termina, gira todo el tiempo evaluando eventos
    {
        Event_t current_event = EV_NONE;

        if (current_state == STATE_IDLE) //Solo en reposo se consumen eventos: los otros estados duran una pasada y no usan el evento
        {
            current_event = get_next_event(); //Consulta (sin bloquear) si hay un evento pendiente
        }
        else //En un estado de atención no se toca ninguna bandera
        {
            current_event = EV_NONE; //Si se consumiera aquí, el evento se perdería sin que nadie lo atienda
        }

        //Safeguard: valida estado y evento antes de usarlos, si alguno está fuera de rango se recupera con STATE_ERROR
        if ((current_state >= NUM_STATES) || (current_event >= NUM_EVENTS)) //Un valor fuera de rango sólo pasa por corrupción de memoria
        {
            current_state = STATE_ERROR; //Se fuerza el estado de error para recuperarse de forma segura
            state_entry_flag = 1U; //El estado de error también ejecuta su [ENTER]
        }

        switch (current_state) //Despacha al código del estado actual
        {
        case STATE_IDLE: //Estado de reposo
            //[ENTER] Corre una sola vez al llegar al estado (aquí no hay nada que preparar)
            if (state_entry_flag) //Solo es 1 en la primera pasada después de llegar al estado
            {
                state_entry_flag = 0U; //Se baja para que [ENTER] no vuelva a ejecutarse mientras se siga en este estado
            }
            //[DO] Corre libremente en cada pasada: en reposo no hace nada, solo espera un evento sin bloquear

            //Transiciones: cada evento lleva a un estado distinto
            if (current_event == EV_ENCODER_CHANGED) {
                current_state = STATE_UPDATE_PWM; //[EXIT] No hay acción de salida en este estado
                state_entry_flag = 1U; //Se marca para que el estado nuevo ejecute su [ENTER]
            } else if (current_event == EV_BUTTON_PRESS) {
                current_state = STATE_CHANGE_MODE; //Pasa a atender el cambio de modo
                state_entry_flag = 1U; //Se marca para que el estado nuevo ejecute su [ENTER]
            } else if (current_event == EV_BLINKY_TICK) {
                current_state = STATE_BLINK; //Pasa a cambiar el LED
                state_entry_flag = 1U; //Se marca para que el estado nuevo ejecute su [ENTER]
            } else if (current_event == EV_CAPTURE) {
                current_state = STATE_UPDATE_CAPT;
                state_entry_flag = 1U;
            }
            break; //Sale del switch, la pasada termina y el while(1) vuelve a empezar

        case STATE_UPDATE_PWM: //Estado que atiende el giro del encoder
            //[ENTER] Acción de entrada (Moore): ajusta el valor seleccionado, una vez por evento del encoder
            if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
            {
                pwm_ApplySteps(encoder_TakeSteps()); //Toma los pasos del encoder y los aplica al valor que indica edit_mode
                state_entry_flag = 0U; //Baja la bandera: la acción ya se ejecutó
            }
            current_state = STATE_IDLE; //Vuelve a esperar eventos
            state_entry_flag = 1U; //IDLE ejecutará su [ENTER]
            break; //Fin del estado

        case STATE_CHANGE_MODE: //Estado que atiende el pulsador
            //[ENTER] Acción de entrada (Moore): cambia de modo, exactamente una vez por cada pulsación
            if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
            {
                mode_Toggle(); //Avanza CCR1 -> CCR3 -> CCR4 -> ajuste RGB -> DAC
                uart_QueueModeReport();
                state_entry_flag = 0U; //Baja la bandera: la acción ya se ejecutó
            }
            current_state = STATE_IDLE; //Vuelve a esperar eventos
            state_entry_flag = 1U; //IDLE ejecutará su [ENTER]
            break; //Fin del estado

        case STATE_BLINK: //Estado que atiende el tick del blinky
            //[ENTER] Acción de entrada (Moore): cambia el LED, exactamente una vez por cada evento del blinky
            if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
            {
                led_Toggle(); //Invierte el estado de PH1
                state_entry_flag = 0U; //Baja la bandera: la acción ya se ejecutó
            }
            current_state = STATE_IDLE; //Vuelve a esperar eventos
            state_entry_flag = 1U; //IDLE ejecutará su [ENTER]
            break; //Fin del estado

        case STATE_UPDATE_CAPT:
            // La ISR calcula el periodo y deja una bandera; la FSM publica el dato al atenderla.
            if (state_entry_flag) {
                periodo_ticks = capture_period_pending;
                frecuencia_hz = (periodo_ticks != 0U)
                                  ? (TIMER_TICK_HZ / periodo_ticks)
                                  : 0U;
                state_entry_flag = 0U;
            }
            current_state = STATE_IDLE;
            state_entry_flag = 1U;
            break;

        case STATE_ERROR:
        default:
            //STATE_ERROR comparte código con default porque la recuperación es la misma
            //Trampa (default:): cualquier estado desconocido se recupera de forma segura hacia el reposo
            current_state = STATE_IDLE; //Reinicia la FSM en un estado conocido
            state_entry_flag = 1U;
            break;
        }

        uart_ReportService();
    }
}

static Event_t get_next_event(void)
{
    //Convierte las banderas de las ISR y el encoder en un evento.
    //Si la ISR del pulsador dejó un evento pendiente
    if (button_flag != 0U) {
        //bajar bandera
        button_flag = 0U;
        return EV_BUTTON_PRESS;
    }

    int32_t counts = (int32_t)__HAL_TIM_GET_COUNTER(&htim5);
    //Con signo: al girar en CCW desde 0, CNT pasa a 0xFFFFFFFF, que es -1
    //si hay almenos un paso completo, CW o CCW
    if ((counts >= COUNTS_PER_STEP) || (counts <= -COUNTS_PER_STEP)) {
        return EV_ENCODER_CHANGED;
    }

    //si la ISR del TIM4 dejó un evento pendiente
    if (blink_flag != 0U) {
        //bajar flag
        blink_flag = 0U;
        return EV_BLINKY_TICK;
    }

    if (capture_flag != 0U) {
        capture_flag = 0U;
        return EV_CAPTURE;
    }

    return EV_NONE;
}

static int32_t encoder_TakeSteps(void)
{
    //Devuelve los pasos completos acumulador (+ CW, - CCW) y los descuenta del TIM5
    int32_t counts = (int32_t)__HAL_TIM_GET_COUNTER(&htim5);
    //División entera: 7 cuentas = 1 paso, -7 cuentas = -1 paso
    int32_t steps = counts / COUNTS_PER_STEP;
    //Se descuentan solo las cuentas usadas, el residuo queda para el siguiente giro
    __HAL_TIM_SET_COUNTER(&htim5, (uint32_t)(counts - steps * COUNTS_PER_STEP));
    return steps;
}

static void pwm_ApplySteps(int32_t steps)
{
    //Suma los pasos al CCR seleccionado o por igual a los tres, con saturación
    switch (edit_mode) {
    case MODE_CCR1: ccr1_value += steps * CCR_STEP; break;
    case MODE_CCR3: ccr3_value += steps * CCR_STEP; break;
    case MODE_CCR4: ccr4_value += steps * CCR_STEP; break;
    case MODE_RGB:
        ccr1_value += steps * CCR_STEP;
        ccr3_value += steps * CCR_STEP;
        ccr4_value += steps * CCR_STEP;
        break;
    case MODE_DAC: ccr2_value += steps * CCR_STEP; break;
    default: return;
    }

    ccr1_value = pwm_ClampCompare(ccr1_value);
    ccr2_value = pwm_ClampCompare(ccr2_value);
    ccr3_value = pwm_ClampCompare(ccr3_value);
    ccr4_value = pwm_ClampCompare(ccr4_value);

    __HAL_TIM_SET_AUTORELOAD(&htim3, (uint32_t)arr_value);
    //Con precarga, los cambios aplican al terminar el periodo actual
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, (uint32_t)ccr1_value);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, (uint32_t)ccr2_value);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, (uint32_t)ccr3_value);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, (uint32_t)ccr4_value);
}

static int32_t pwm_ClampCompare(int32_t value)
{
    int32_t maximum = arr_value + 1;
    if (value < 0) return 0;
    if (value > maximum) return maximum;
    return value;
}

static void mode_Toggle(void)
{
    //Avanza por CCR1, CCR3, CCR4 y el ajuste común RGB
    edit_mode = (Mode_t)((edit_mode + 1) % NUM_MODES);
    mode_Indicators_Update();
}

static void mode_Indicators_Update(void)
{
    //Enciende los indicadores correspondientes al modo activo
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12, GPIO_PIN_RESET);
    switch (edit_mode) {
    case MODE_CCR1: HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET); break;
    case MODE_CCR3: HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET); break;
    case MODE_CCR4: HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_SET); break;
    case MODE_RGB:
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12, GPIO_PIN_SET);
        break;
    case MODE_DAC: break;
    default: break;
    }
}

static void led_Toggle(void)
{
    //XOR con 1: invierte el bit sin tocar los otros pines
    HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_1);
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

static void gpio_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOH, &gpio);
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_RESET);

    gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOC, &gpio);
    mode_Indicators_Update();

    /* EXTI0: PB0, pulsador activo en bajo. */
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_NVIC_SetPriority(EXTI0_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

static void tim3_pwm_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    TIM_OC_InitTypeDef oc = {0};

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &gpio);

    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 15U;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = ARR_INIT;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) Error_Handler();

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = CCR_INIT;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_2) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_3) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_4) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4) != HAL_OK) Error_Handler();
}

static void tim4_Init(void)
{
    __HAL_RCC_TIM4_CLK_ENABLE();
    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 15U;  /* 16 MHz / 16 = 1 MHz */
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = 999U;    /* 1 kHz update: one callback per millisecond */
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim4) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(TIM4_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK) Error_Handler();
}

static void tim5_encoder_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    TIM_Encoder_InitTypeDef encoder = {0};

    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM5;
    HAL_GPIO_Init(GPIOA, &gpio);

    __HAL_RCC_TIM5_CLK_ENABLE();
    htim5.Instance = TIM5;
    htim5.Init.Prescaler = 0U;
    htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim5.Init.Period = 0xFFFFFFFFU;
    htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    encoder.EncoderMode = TIM_ENCODERMODE_TI12;
    encoder.IC1Polarity = TIM_ICPOLARITY_RISING;
    encoder.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    encoder.IC1Prescaler = TIM_ICPSC_DIV1;
    encoder.IC1Filter = 15U;
    encoder.IC2Polarity = TIM_ICPOLARITY_RISING;
    encoder.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    encoder.IC2Prescaler = TIM_ICPSC_DIV1;
    encoder.IC2Filter = 15U;
    if (HAL_TIM_Encoder_Init(&htim5, &encoder) != HAL_OK) Error_Handler();
    if (HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL) != HAL_OK) Error_Handler();
}

static void tim2_capture_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    TIM_IC_InitTypeDef capture = {0};

    gpio.Pin = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOB, &gpio);

    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 15U; /* 1 MHz counter from the 16 MHz APB1 timer clock */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFFFFFFU;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_IC_Init(&htim2) != HAL_OK) Error_Handler();

    capture.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    capture.ICSelection = TIM_ICSELECTION_DIRECTTI;
    capture.ICPrescaler = TIM_ICPSC_DIV1;
    capture.ICFilter = 0U;
    if (HAL_TIM_IC_ConfigChannel(&htim2, &capture, TIM_CHANNEL_3) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(TIM2_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3) != HAL_OK) Error_Handler();
}

static void usart2_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    __HAL_RCC_USART2_CLK_ENABLE();
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200U;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(USART2_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void uart_ReportService(void)
{
    if (HAL_UART_GetState(&huart2) != HAL_UART_STATE_READY) return;

    if (uart_mode_queue_count > 0U) {
        static const char *const mode_names[NUM_MODES] = {
            "CCR1", "CCR3", "CCR4", "RGB", "DAC"
        };
        UartModeReport_t report = uart_mode_queue[uart_mode_queue_head];
        int length = snprintf(uart_tx_buffer, sizeof(uart_tx_buffer),
                              "Modo: %s | Frecuencia: %lu Hz\r\n",
                              mode_names[report.mode],
                              (unsigned long)report.frequency_hz);
        if ((length > 0) && ((size_t)length < sizeof(uart_tx_buffer)) &&
            (HAL_UART_Transmit_IT(&huart2, (uint8_t *)uart_tx_buffer, (uint16_t)length) == HAL_OK)) {
            uart_mode_queue_head = (uint8_t)((uart_mode_queue_head + 1U) % MODE_REPORT_QUEUE_LEN);
            uart_mode_queue_count--;
        }
        return;
    }

}

static void uart_QueueModeReport(void)
{
    if (uart_mode_queue_count >= MODE_REPORT_QUEUE_LEN) {
        return;
    }

    uart_mode_queue[uart_mode_queue_tail].mode = edit_mode;
    uart_mode_queue[uart_mode_queue_tail].frequency_hz = frecuencia_hz;
    uart_mode_queue_tail = (uint8_t)((uart_mode_queue_tail + 1U) % MODE_REPORT_QUEUE_LEN);
    uart_mode_queue_count++;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) {
        if (button_debounce_ms > 0U) {
            if (button_wait_release != 0U) {
                if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET) {
                    button_debounce_ms--;
                } else {
                    button_debounce_ms = BUTTON_DEBOUNCE_MS;
                }
                if (button_debounce_ms == 0U) {
                    button_wait_release = 0U;
                    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
                    HAL_NVIC_ClearPendingIRQ(EXTI0_IRQn);
                    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
                }
            } else {
                button_debounce_ms--;
                if (button_debounce_ms == 0U) {
                    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) {
                        button_flag = 1U;
                    }
                    button_wait_release = 1U;
                    button_debounce_ms = BUTTON_DEBOUNCE_MS;
                }
            }
        }

        blink_count++;
        if (blink_count >= BLINK_HALF_PERIOD_MS) {
            blink_count = 0U;
            blink_flag = 1U;
        }
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0) {
        HAL_NVIC_DisableIRQ(EXTI0_IRQn);
        button_wait_release = 0U;
        button_debounce_ms = BUTTON_DEBOUNCE_MS;
    }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if ((htim->Instance == TIM2) && (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)) {
        uint32_t capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
        if (capture_previous_valid != 0U) {
            capture_period_pending = capture - capture_previous;
            capture_flag = 1U;
        } else {
            capture_previous_valid = 1U;
        }
        capture_previous = capture;
    }
}

static void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}

//Centinela: cantidad de eventos, sirve para validar el rango
/* ============= VARIABLES ============= */
// TIM4 mantiene el antirebote mientras EXTI0 permanece enmascarada.
//GPIOB PB0 + EXTI0 (flanco de bajada)
//Evento que se evalúa en cada pasada del ciclo infinito
//Se giró el encoder
//Se presionó el pulsador
//Venció el medio periodo del blinky
//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
/* ========= Funciones driver de la FSM (el cómo) ========= */
/* ========== Fuinciones de inicialización del hardware (se asume ele stado de reset de cada registro) ========== */
//PH1 como blinky y PC10/PC12/PC11 como indicadores de modo
//Señal de reloj del GPIOH
//01: SALIDA
//Señal de reloj del GPIOC
//PINES PC10, PC11 Y PC12 COMO SALIDA
//Interrupción cada 1 ms: PSC = 15 -> tick de 1 us, ARR = 999 -> 1000 us
//Señal de reloj del TIM4
//16 MHz / 16 = 1 MHz
//CUenta de 0 a 999 (ARR = 0 bloquearía el contador)
//Habilita la interrupción por actualización (primer interruptor)
//Habilita la interrupción ene l NVIC (segundo interruptor)
//Arranca el contador
//PC6/CH1, PC7/CH2, PC8/CH3 y PC9/CH4 (AF02): PWM modo 1, 500 Hz, duty inicial 50%
//RELOJ DEL GPIOC
//AF2 para PC6 y PC7 (AFR[0] contiene los pines 0..7)
//AF2 para PC8 y PC9 (AFR[1] contiene los pines 8..)
//RELOJ DEL TIM3
//Tick de 1 us
//PERIODO (1999 + 1) us
//Ancho inicial del pulso de PC6: 50%
//Ancho inicial del pulso de PC7: 50%
//Ancho inicial del pulso de PC8: 50%
//Ancho inicial del pulso de PC9: 50%
//ARR CON PRECARGA
//110: PWM MODO 1, ALTO MIENTRAS CNT < CCR1 PARA CH1 Y CH2
//CCR1 CON PRECARGA PARA CH1 Y CH2
//enable CH1, CH2, CH3 Y CH4
//Carga PSC, ARR y CCR1/3/4 a los registros activos
//ARRANCA EL CONTADOR
//PA0 ->  TIM5_CH1 (CLK), PA1 -> TIM5_CH2 (DT), AF2 CON PULL-UP. MODI ENCODER: 4 CUENTAS POR PASO
//RELOJ DEL GPIOA
//10: FUNCIÓN ALTERNA
//01: PULL-UP
//AF2
//RELOJ DEL TIM5
//TIM5 ES DE 32 BIT: NO SE DESBORDA EN USO NORMAL
//01: CH1 LEE TI1 Y CH2 LEE T12
//FILTRO DIGITAL MÁXIMO
//HABILITA LA CAPTURA DE LOS DOS CANALES
//011: MODOD ENCODER 3, CUENTA LOS FLANCOS DE TI1 Y TI2
//ARRANCAR EL CONTADOR
//PB0 (SW DEL KY040) -> EXTI0, FLANCO DE BAJADA
//Reloj del GPIOB (PB0 queda como entrada, estado de reset)
//01: pull-up, en reposo lee 1 y al presionar baja a 0
//Reloj del SYSCFG, el multiplexor de EXTI vive ahí
//La línea EXTI0 toma el pin 0 del puerto B
//Disparo por flanco de bajada
//Desenmascara la línea 0
//Habilita la interrupción en el NVIC
// PB10: FUNCIÓN ALTERNATIVA AF01 (TIM2_CH3)
//10: Alternate function mode
//0001: AF1
// TIM2 es de 32 bits
//CC3S:00: noninverted/rising edge
//01: CH3 LEE TI3
//1: Capture enabled
//1: Counter enabled
/* ======= FUNCIONES ISR (SOLO REGISTRAN EL EVENTO Y REGRESAN, SIN LÓGICA DE LA FSM) ======= */
//Cada 1 ms cuenta el milisegundo y cada blink_half_period_ms deja la bandera del blinky
// Antirebote no bloqueante del pulsador. EXTI0 está enmascarada desde el primer flanco.
// Exige que el botón permanezca liberado durante BUTTON_DEBOUNCE_MS.
//Descarta cualquier pendiente generado durante el rebote
//Rearma EXTI0 para la siguiente pulsación
//Una pulsación validada; la FSM la procesa una sola vez
// El flanco fue un rebote: rearma solo después de comprobar que sigue liberado.
//La FSM lo convierte en EV_BLINK_TICK
//Pulsador por EXTI0: confirma la pulsación y la liberación con el tick de 1 ms de TIM4.
//Enmascara nuevos flancos durante el antirebote
//Comienza la validación de la pulsación
//Lectura CCR3 reconoce la captura pendiente
// Aritmética unsigned: la resta también resuelve un desbordamiento del contador.
