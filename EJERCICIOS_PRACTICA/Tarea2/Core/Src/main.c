/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Steven A. Oquendo Rave
 * @brief          : Encoder KY040 (TIM5) ajusta CCR1, CCR3, CCR4, los tres canales
 *                   RGB o CH2/DAC del PWM (TIM3); EXTI0 selecciona el modo.
 *                   El programa está organizado como una Máquina de Estados Finitos (FSM).
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
 * - Indicadores de modo: PC10 (rojo), PC12 (verde), PC11 (azul).
 *   CCR1: rojo; CCR3: verde; CCR4: azul; RGB: los tres; DAC: todos apagados intencionalmente.
 * - CH2 (PWM) -> PC7 (DAC)
 *
 ** ENCODER TIM5 - AF02:
 * - CH1 (KY040: CLK) -> PA0
 * - CH2 (KY040: DT)  -> PA1
 *
 ** PULSADOR KY040 (SW), activo en bajo:
 * - SW -> PB0 (EXTI0)
 *
 ** BLINKY TIM4 -> PH1: cambia de estado cada 125 ms y completa un ciclo cada 250 ms (4 Hz).
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
 *  dura 1 us: periodo = (ARR + 1) us. Arranca con ARR = 1999 y los cuatro
 *  canales (CH1/PC6, CH2/PC7, CH3/PC8 y CH4/PC9) con CCR = 1000: 500 Hz y
 *  duty de 50 % en cada salida.
 *
 * -Hay cinco modos de edición y cada flanco de bajada del pulsador avanza al siguiente:
 *  Los cinco modos son MODE_CCR1, MODE_CCR3, MODE_CCR4, MODE_RGB y MODE_DAC.
 *  En CCR1/CCR3/CCR4 cada paso ajusta el canal seleccionado; RGB ajusta por igual
 *  CH1, CH3 y CH4; DAC ajusta CH2. Cada paso suma o resta 10 al ancho del pulso.
 *  Los CCR se limitan a 0..ARR + 1. Los indicadores muestran rojo para CCR1,
 *  verde para CCR3, azul para CCR4, los tres para RGB y todos apagados
 *  intencionalmente para DAC.
 *
 * -TIM4 es la base de tiempo: interrumpe cada 1 ms y cuenta milisegundos. PH1 cambia
 *  de estado cada 125 ms y completa un ciclo cada 250 ms, por lo que parpadea siempre a 4 Hz en todos los modos.
 *
 * -TIM2 captura cada flanco de subida en CCR3. La resta entre capturas consecutivas
 *  deja el periodo en periodo_ticks (ticks de 1 us con el reloj TIM2 actual).
 *
 * -Reloj de TIM2: PSC = 15 supone un reloj de entrada de 16 MHz y genera ticks de 1 us.
 *
 *
 *===================== FSM (presentación 05-FSM) ===================== *
 *
 *  patrón 1: Switch-Case. La FSM decide qué pasa (política), las funciones
 *  driver hacen el cómo (hardware). TIM2 calcula el período entre capturas; TIM4
 *  actualiza el antirebote y el conteo del blinky; EXTI0 enmascara su línea e inicia
 *  el antirebote. La FSM procesa las banderas en el ciclo principal mediante
 *  get_next_event(), y el while(1) nunca se bloquea.
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

#include "stm32f411xe.h"
#include <assert.h>
#include <stdint.h>
#include <stm32f4xx.h>
#include <sys/types.h>

// Constantes

#define COUNTS_PER_STEP	4       //El modo TI12 cuenta 4 flancos por cada paso mecánico del encoder
#define ARR_INIT		1999    //ARR inicial: (1999 + 1) x 1 us = 2000 us -> 500 Hz
#define CCR_INIT		((ARR_INIT + 1) / 2) //La mitad del periodo: 1000 us de 2000 us -> duty de 50 %
#define CCR_STEP		10      //Lo que suma o resta al CCR seleccionado en cada paso
#define BLINK_HALF_PERIOD_MS	125 //PH1 cambia cada 125 ms: periodo de 250 ms -> 4 Hz
#define BUTTON_DEBOUNCE_MS	20  //Tiempo estable requerido para validar la pulsación y la liberación

/* ========= MÁQUINA DE ESTADOS FINITOS =========*/

//  Paso 1 (FSM): mapear el vocabulario a memoria. Estados y eventos son enum (type safety)
//  y NUM_STATES /NUM_EVENTS son centinelas para validar rangos.

//  MODOS DE EDICIÓN: qué valor del PWM modifica el encoder; NUM_MODES es el centinela, no un modo seleccionable.
typedef enum {      //Lista de modos posibles
    MODE_CCR1,      //El encoder cambia el ancho de pulso de PC6 / TIM3_CH1
    MODE_CCR3,      //El encoder cambia el ancho de pulso de PC8 / TIM3_CH3
    MODE_CCR4,      //El encoder cambia el ancho de pulso de PC9 / TIM3_CH4
    MODE_RGB,       //El encoder cambia por igual los tres anchos de pulso
	MODE_DAC,		//El encoder cambia el ancho de pulso de PC7 / TIM3_CH2
    NUM_MODES       //Cantidad de modos; se usa para validar el rango y avanzar cíclicamente
} Mode_t;           //Nombre del tipo: edit_mode solo puede recibir valores de este enum

//ESATDOS: la condición en la que está el sistema en un instante dado
typedef enum {          //Lista de estados posibles, cada nombre vale un entero (0, 1, 2...)
    STATE_IDLE,         //Reposo: espera un evento sin hacer nada más (no bloquea)
    STATE_UPDATE_PWM,   //Atiende el giro: ajusta el CCR seleccionado
    STATE_CHANGE_MODE,  //Atiende el pulsador: avanza entre los cinco modos
    STATE_BLINK,       	//Atiende el tick del TIM4: cambia el estado del LED PH1
	STATE_UPDATE_CAPT,  //Atiende una captura pendiente de la señal de entrada
    STATE_ERROR,        //Estado inválido (tranpa de hardware): se recupera hacia STATE_IDLE
    NUM_STATES          //Centinela: cantidad de estados, sirve para validar el rango
}   State_t;            //Nombre del tipo: current_state solo puede recibir valores de este enum

//EVENTOS: los disparadores (ISR o lecturas ) que provocan transiciones entre estados
typedef enum {          //Lista de eventos posibles, cada nombre vale un entero (0, 1, 2...)
    EV_NONE,            //No ha ocurrido nada
    EV_ENCODER_CHANGED, //El encoder avanzó o retrocedió al menos un paso
    EV_BUTTON_PRESS,    //El pulsador del KY040 fue presionado (flanco de bajada)
    EV_BLINKY_TICK,     //Venció el medio periodo del blinky (125 ms)
	EV_CAPTURE,         //Hay un periodo capturado pendiente de procesar
    NUM_EVENTS          //Centinela: cantidad de eventos, sirve para validar el rango
}   Event_t;            //Nombre del ripo: current_event solo puede recibir valores de este enum

/* ============= VARIABLES ============= */

//Estado actual de la FSM, static: solo se accede desde este módulo (scope mínimo) y solo se modifica en main
static State_t current_state = STATE_IDLE;  //Siempre iniicia en reposo
//Bandera de ENTRY: vale 1 al llegar a un estado para que su acción [ENTER] se ejecute exactamente una vez
static uint8_t state_entry_flag = 1;        //Inicia en 1 para que el primer estado también ejecute su [ENTER]


volatile uint8_t  button_flag = 0;      //Bandera que deja la ISR del pulsador (volatile: cambia fuera del flujo normal del programa)
volatile uint8_t  blink_flag  = 0;      //Bandera que deja la ISR del TIM4 (volatile por la misma razoń)

// Período y frecuencia más recientes, publicados por la FSM tras una captura válida.
volatile uint32_t periodo_ticks;          //Último periodo procesado por la FSM, expresado en ticks (1us)
volatile uint32_t frecuencia_hz;          //Frecuencia calculada en Hz (resolución de 1 Hz)
// Período calculado por TIM2; captura_flag notifica a la FSM que hay un dato nuevo.
static volatile uint32_t captura_pendiente_ticks;
static volatile uint8_t captura_flag;
// La primera captura inicializa la referencia; las siguientes se restan para obtener el período.
static uint32_t captura_anterior;
static uint8_t captura_previa_valida;

// TIM4 mantiene el antirebote mientras EXTI0 permanece enmascarada.
static volatile uint8_t button_debounce_ms = 0;
static volatile uint8_t button_wait_release = 0;
static const uint16_t blink_half_period_ms = BLINK_HALF_PERIOD_MS; //Milisegundos entre cambios del LED PH1

static Mode_t edit_mode = MODE_CCR1;   //Modo inicial: editar PC6 / TIM3_CH1
int32_t arr_value = ARR_INIT;           //ARR que tiene el TIM3 (periodo = ARR +  1 ticks de 1 us), con signo para poder comparar con los límites
int32_t ccr1_value = CCR_INIT;          //Ancho de pulso de PC6, inicia en 50%
int32_t ccr2_value = CCR_INIT;			//Ancho de pulso de PC7, inicia en 50%
int32_t ccr3_value = CCR_INIT;          //Ancho de pulso de PC8, inicia en 50%
int32_t ccr4_value = CCR_INIT;          //Ancho de pulso de PC9, inicia en 50%

/*  =========== Definición de funciones ===========*/

//Drivers de la FSM (el cómo)
static Event_t get_next_event(void);        //lee (y limpia) las banderas de la ISR y la lectura del encoder, y las convierte en un evento
static int32_t encoder_TakeSteps(void);     //Devuelve los paso acumulados (con signo) y los descuenta del TIM5
static void pwm_ApplySteps (int32_t steps); //Ajusta CCR1, CCR3, CCR4, CH2/DAC o los tres canales RGB, con saturación
static void mode_Toggle(void);              //Avanza entre los cinco modos
static void mode_Indicators_Update(void);   //Actualiza los LED indicadores según el modo activo
static void led_Toggle(void); 				//Cambia el estado del LED PH1

//Inicialización del hardware
static void gpio_Init(void);                //GPIOH PH1 y GPIOC PC10/PC11/PC12 como salidas
static void tim4_Init(void);                //TIM4 con interrupción cada 1 ms (base de tiempo del blinky)
static void tim3_pwm_Init(void);            //GPIOC PC6(TIM3_CH1), PC7(TIM3_CH2), PC8(TIM3_CH3) Y PC9(TIM3_CH4) en modo PWM 1
static void tim5_encoder_Init(void);        //GPIOA PA0, PA1 + TIM5 en modo encoder
static void tim2_capture_Init(void);		//GPIOB PB10 en modo captura

static void exti0_Init(void); //GPIOB PB0 + EXTI0 (flanco de bajada)

/* ========== Main ========== */
int main(void) 
{
	Event_t current_event = EV_NONE; //Evento que se evalúa en cada pasada del ciclo infinito

	gpio_Init(); 			//Configura PH1 (blinky)
	tim3_pwm_Init(); 		//Configura las cuatro salidas PWM de TIM3
	tim5_encoder_Init(); 	//Configura el encoder
	exti0_Init(); 			//Configura el pulsador por EXTI0 en PB0
	tim4_Init(); 			//Último: las interrupciones del blinky empiezan con todo el hardware listo
	tim2_capture_Init();	//Configura la captura

	/* Patrón 1 (FSM): Switch-Case. El ciclo infinito nunca termina: lee un evento, evalúa el estado y reacciona.
	 * Regla de NO bloqueo: no hay delay(), cada pasada procesa a lo sumo UN evento y UNA transición. */
	while (1) //El firmware nunca termina, gira todo el tiempo evaluando eventos
	{
		if (current_state == STATE_IDLE) 		//Solo en reposo se consumen eventos: los otros estados duran una pasada y no usan el evento
		{
			current_event = get_next_event(); 	//Consulta (sin bloquear) si hay un evento pendiente
		}
		else 									//En un estado de atención no se toca ninguna bandera
		{
			current_event = EV_NONE; 			//Si se consumiera aquí, el evento se perdería sin que nadie lo atienda
		}

		//Safeguard: valida estado y evento antes de usarlos, si alguno está fuera de rango se recupera con STATE_ERROR
		if ((current_state >= NUM_STATES) || (current_event >= NUM_EVENTS)) 
		{
			current_state = STATE_ERROR; 		//Se fuerza el estado de error para recuperarse de forma segura
			state_entry_flag = 1; 				//El estado de error también ejecuta su [ENTER]
		}

		switch (current_state) 					//Despacha al código del estado actual
		{

		case STATE_IDLE: 						//Estado de reposo
			//[ENTER] Corre una sola vez al llegar al estado (aquí no hay nada que preparar)
			if (state_entry_flag) 				//Solo es 1 en la primera pasada después de llegar al estado
			{
				state_entry_flag = 0; 			//Se baja para que [ENTER] no vuelva a ejecutarse mientras se siga en este estado
			}

			//Transiciones: cada evento lleva a un estado distinto
			if (current_event == EV_ENCODER_CHANGED) 	//Se giró el encoder
			{
				current_state = STATE_UPDATE_PWM; 		//[EXIT] No hay acción de salida en este estado
				state_entry_flag = 1; 					//Se marca para que el estado nuevo ejecute su [ENTER]
			}
			else if (current_event == EV_BUTTON_PRESS) 	//Se presionó el pulsador
			{
				current_state = STATE_CHANGE_MODE; 		//Pasa a atender el cambio de modo
				state_entry_flag = 1; 					//Se marca para que el estado nuevo ejecute su [ENTER]
			}
			else if (current_event == EV_BLINKY_TICK) 	//Venció el medio periodo del blinky
			{
				current_state = STATE_BLINK; 			//Pasa a cambiar el LED
				state_entry_flag = 1; 					//Se marca para que el estado nuevo ejecute su [ENTER]
			}
			else if (current_event == EV_CAPTURE)
			{
				current_state = STATE_UPDATE_CAPT; 		//Pasa a publicar el período y actualizar la frecuencia
				state_entry_flag = 1; 					//Ejecuta una sola vez la acción de entrada del estado nuevo
			}
			break; 										//Sale del switch, la pasada termina y el while(1) vuelve a empezar

		case STATE_UPDATE_PWM: 							//Estado que atiende el giro del encoder
			//[ENTER] Acción de entrada: ajusta el valor seleccionado, una vez por evento del encoder
			if (state_entry_flag) 						//Solo la primera pasada después de llegar a este estado
			{
				pwm_ApplySteps(encoder_TakeSteps()); //Toma los pasos del encoder y los aplica al valor que indica edit_mode
				state_entry_flag = 0; 					//Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; 				//Vuelve a esperar eventos
			state_entry_flag = 1; 						//IDLE ejecutará su [ENTER]
			break; 										//Fin del estado

		case STATE_CHANGE_MODE: 						//Estado que atiende el pulsador
			//[ENTER] Acción de entrada: cambia de modo, exactamente una vez por cada pulsación
			if (state_entry_flag) 						//Solo la primera pasada después de llegar a este estado
			{
				mode_Toggle(); 							//Avanza CCR1 -> CCR3 -> CCR4 -> ajuste RGB -> DAC
				state_entry_flag = 0; 					//Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; 				//Vuelve a esperar eventos
			state_entry_flag = 1; 						//IDLE ejecutará su [ENTER]
			break; 										//Fin del estado

		case STATE_BLINK: 								//Estado que atiende el tick del blinky
			//[ENTER] Acción de entrada: cambia el LED, exactamente una vez por cada evento del blinky
			if (state_entry_flag) 						//Solo la primera pasada después de llegar a este estado
			{
				led_Toggle(); 							//Invierte el estado de PH1
				state_entry_flag = 0; 					//Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; 				//Vuelve a esperar eventos
			state_entry_flag = 1; 						//IDLE ejecutará su [ENTER]
			break; 										//Fin del estado

		case STATE_UPDATE_CAPT: 						//Atiende el período pendiente de captura
			// TIM2 calcula el período entre flancos; la FSM lo publica y deriva la frecuencia.
			if (state_entry_flag)
			{
				periodo_ticks = captura_pendiente_ticks; //Copia el período que dejó la ISR
				frecuencia_hz = (periodo_ticks != 0U)
						? (1000000U / periodo_ticks)
						: 0U;
				// TIM2 cuenta a 1 MHz; el caso cero evita dividir entre cero.
				state_entry_flag = 0; 					//Evita repetir la acción de entrada
			}
			current_state = STATE_IDLE; 				//La captura ya fue atendida; vuelve a esperar eventos
			state_entry_flag = 1; 						//Prepara la próxima entrada a IDLE
			break;

		case STATE_ERROR: 								//STATE_ERROR comparte código con default porque la recuperación es la misma
		default: 										//Trampa (default:): cualquier estado desconocido se recupera de forma segura hacia el reposo
			current_state = STATE_IDLE; 				//Reinicia la FSM en un estado conocido
			state_entry_flag = 1; 						//IDLE ejecutará su [ENTER]
			break; 										//Fin del estado
		}
	}
}


/* ========= Funciones driver de la FSM (el cómo) ========= */

//Convierte las banderas de las ISR y el encoder en un evento.
static Event_t get_next_event(void){
    // Prioridad: botón, encoder, blinky y, por último, captura pendiente.
    if (button_flag == 1)   						//Si la ISR del pulsador dejó un evento pendiente
    {
        button_flag = 0;    						//bajar bandera
        return EV_BUTTON_PRESS;
    }

    int32_t counts = (int32_t) TIM5->CNT;   		//Con signo: al girar en CCW desde 0, CNT pasa a 0xFFFFFFFF, que es -1
    if ((counts >= COUNTS_PER_STEP) || (counts <= -COUNTS_PER_STEP))    //si hay almenos un paso completo, CW o CCW
    {
        return EV_ENCODER_CHANGED;
    }

    if (blink_flag ==1) 							//si la ISR del TIM4 dejó un evento pendiente
    {
        blink_flag = 0; 							//bajar flag
        return EV_BLINKY_TICK;
    }

	// La captura se consume tras los eventos de mayor prioridad.
	if (captura_flag == 1)
	{
		captura_flag = 0; 							//Consume la notificación para no procesar dos veces el mismo período
		return EV_CAPTURE; 							//La FSM publicará el período pendiente
	}

    return EV_NONE; 								//No hay evento pendiente ni movimiento completo del encoder
}

//Devuelve los pasos completos acumulador (+ CW, - CCW) y los descuenta del TIM5
static int32_t encoder_TakeSteps(void)
{
    int32_t steps = (int32_t) TIM5->CNT / COUNTS_PER_STEP;  //División entera: 7 cuentas = 1 paso, -7 cuentas = -1 paso
    TIM5->CNT -= steps * COUNTS_PER_STEP;   				//Se descuentan solo las cuentas usadas, el residuo queda para el siguiente giro
    return steps;
}

//Suma los pasos al CCR seleccionado o por igual a los tres, con saturación
static void pwm_ApplySteps(int32_t steps)
{
    // Ajusta el CCR del modo activo; RGB aplica el mismo cambio a CH1, CH3 y CH4.
    switch (edit_mode)
    {
    case MODE_CCR1:
        ccr1_value += steps * CCR_STEP;
        break;
    case MODE_CCR3:
        ccr3_value += steps * CCR_STEP;
        break;
    case MODE_CCR4:
        ccr4_value += steps * CCR_STEP;
        break;
    case MODE_RGB:
        ccr1_value += steps * CCR_STEP;
        ccr3_value += steps * CCR_STEP;
        ccr4_value += steps * CCR_STEP;
        break;
	case MODE_DAC: //El modo DAC edita CH2/PC7
		ccr2_value += steps * CCR_STEP;
		break;
    default:
        return;
    }

    // Limita cada CCR al intervalo válido de 0 a ARR+1.
    if (ccr1_value < 0)
    {
        ccr1_value = 0;
    }
    if (ccr1_value > arr_value + 1)
    {
        ccr1_value = arr_value + 1;
    }
	if (ccr2_value < 0)
	{
		ccr2_value = 0;
	}
	if (ccr2_value > arr_value +1) 
	{
		ccr2_value = arr_value +1;
	}
    if (ccr3_value < 0)
    {
        ccr3_value = 0;
    }
    if (ccr3_value > arr_value + 1)
    {
        ccr3_value = arr_value + 1;
    }
    if (ccr4_value < 0)
    {
        ccr4_value = 0;
    }
    if (ccr4_value > arr_value + 1)
    {
        ccr4_value = arr_value + 1;
    }

    // Escribe el período y los cuatro anchos; la precarga aplica los cambios al terminar el período actual.
    TIM3->ARR  = (uint32_t) arr_value;  //Con precarga, los cambios aplican al terminar el periodo actual
    TIM3->CCR1 = (uint32_t) ccr1_value;
	TIM3->CCR2 = (uint32_t) ccr2_value;
    TIM3->CCR3 = (uint32_t) ccr3_value;
    TIM3->CCR4 = (uint32_t) ccr4_value;
}

//Avanza cíclicamente por CCR1, CCR3, CCR4, el ajuste común RGB y DAC.
static void mode_Toggle(void)
{
    edit_mode = (Mode_t) ((edit_mode + 1) % NUM_MODES); //Pasa al siguiente modo y vuelve a CCR1 al superar DAC
    mode_Indicators_Update(); 							//Refleja el nuevo modo en los LED indicadores
}

//Enciende los indicadores correspondientes al modo activo
static void mode_Indicators_Update(void)
{
    // Primero prepara el patrón apagado; después selecciona los indicadores del modo.
    uint32_t indicators = 0;

    switch (edit_mode)
    {
    case MODE_CCR1:
        indicators = GPIO_BSRR_BS10;
        break;
    case MODE_CCR3:
        indicators = GPIO_BSRR_BS12;
        break;
    case MODE_CCR4:
        indicators = GPIO_BSRR_BS11;
        break;
    case MODE_RGB:
        indicators = GPIO_BSRR_BS10 | GPIO_BSRR_BS11 | GPIO_BSRR_BS12;
        break;
    default: //MODE_DAC mantiene los tres indicadores apagados
        break;
    }

    GPIOC->BSRR = (GPIO_BSRR_BR10) | (GPIO_BSRR_BR11) | (GPIO_BSRR_BR12) | indicators; //Apaga los tres y activa el patrón elegido en una escritura
}

//Cambia el estado del LED PH1
static void led_Toggle(void)
{
    GPIOH->ODR ^= GPIO_ODR_OD1;     //XOR con 1: invierte el bit sin tocar los otros pines
}

/* ========== Fuinciones de inicialización del hardware (se asume ele stado de reset de cada registro) ========== */

//PH1 como blinky y PC10/PC12/PC11 como indicadores de modo
static void gpio_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN;    //Señal de reloj del GPIOH
    GPIOH->MODER |= GPIO_MODER_MODE1_0;     //01: configura PH1 como salida para el LED

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;    													//Señal de reloj del GPIOC
    GPIOC->MODER &= ~((GPIO_MODER_MODE10) | (GPIO_MODER_MODE11) | (GPIO_MODER_MODE12)); 		//Limpia el modo previo de los tres pines
    GPIOC->MODER |= ((GPIO_MODER_MODE10_0) | (GPIO_MODER_MODE11_0) | (GPIO_MODER_MODE12_0)); 	//Configura PC10, PC11 y PC12 como salidas
    mode_Indicators_Update(); 																	//Muestra el patrón correspondiente al modo inicial CCR1
}

//Interrupción cada 1 ms: PSC = 15 -> tick de 1 us, ARR = 999 -> 1000 us
static void tim4_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;     //Señal de reloj del TIM4

    TIM4->PSC = 15;                         //16 MHz / 16 = 1 MHz
    TIM4->ARR = 999;                        //CUenta de 0 a 999 (ARR = 0 bloquearía el contador)

    TIM4->DIER |= TIM_DIER_UIE;             //Habilita la interrupción por actualización (primer interruptor)
    __NVIC_EnableIRQ(TIM4_IRQn);       //Habilita la interrupción ene l NVIC (segundo interruptor)

    TIM4->CR1 |= TIM_CR1_CEN;               //Arranca el contador
}

//PC6/CH1, PC7/CH2, PC8/CH3 y PC9/CH4 (AF02): PWM modo 1, 500 Hz, duty inicial 50%
static void tim3_pwm_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;    //RELOJ DEL GPIOC
    GPIOC->MODER |= (GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1 | GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1);
    GPIOC->AFR[0] |= (GPIO_AFRL_AFRL6_1  | GPIO_AFRL_AFRL7_1);      //AF2 para PC6 y PC7 (AFR[0] contiene los pines 0..7)
    GPIOC->AFR[1] |= (GPIO_AFRH_AFSEL8_1 | GPIO_AFRH_AFSEL9_1); 	//AF2 para PC8 y PC9 (AFR[1] contiene los pines 8..)

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;     //RELOJ DEL TIM3

    TIM3->PSC  = 15;            //Tick de 1 us
    TIM3->ARR  = ARR_INIT;      //PERIODO (1999 + 1) us
    TIM3->CCR1 = CCR_INIT;      //Ancho inicial del pulso de PC6: 50%
	TIM3->CCR2 = CCR_INIT;		//Ancho inicial del pulso de PC7: 50%
    TIM3->CCR3 = CCR_INIT;      //Ancho inicial del pulso de PC8: 50%
    TIM3->CCR4 = CCR_INIT;      //Ancho inicial del pulso de PC9: 50%
    TIM3->CR1 |= TIM_CR1_ARPE;  //ARR CON PRECARGA

    TIM3->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2);     //110: PWM modo 1 para CH1 y CH2, salida activa mientras CNT < CCR
    TIM3->CCMR1 |= (TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE);		//Habilita precarga de CCR1 y CCR2 para CH1 y CH2
    TIM3->CCMR2 |= (TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2);		//110: PWM modo 1 para CH3 y CH4, salida activa mientras CNT < CCR
    TIM3->CCMR2 |= (TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE);		//Habilita precarga de CCR3 y CCR4 para CH3 y CH4
    TIM3->CCER  |= (TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E);		//enable CH1, CH2, CH3 Y CH4
    
    TIM3->EGR    = TIM_EGR_UG;          //Carga PSC, ARR y CCR1/2/3/4 a los registros activos
    TIM3->CR1   |= TIM_CR1_CEN;         //ARRANCA EL CONTADOR
}

//PA0 ->  TIM5_CH1 (CLK), PA1 -> TIM5_CH2 (DT), AF2 CON PULL-UP. MODI ENCODER: 4 CUENTAS POR PASO
static void tim5_encoder_Init(void)
{
    RCC->AHB1ENR  |= RCC_AHB1ENR_GPIOAEN;    //RELOJ DEL GPIOA
    GPIOA->MODER  |= (GPIO_MODER_MODER0_1 | GPIO_MODER_MODER1_1);   //10: FUNCIÓN ALTERNA
    GPIOA->PUPDR  |= (GPIO_PUPDR_PUPD0_0 | GPIO_PUPDR_PUPD1_0);     //01: PULL-UP
    GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL0_1 | GPIO_AFRL_AFSEL1_1);     //AF2

    RCC->APB1ENR  |= RCC_APB1ENR_TIM5EN;    //RELOJ DEL TIM5

    TIM5->ARR = 0xFFFFFFFF;     //TIM5 ES DE 32 BIT: NO SE DESBORDA EN USO NORMAL
    TIM5->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0);   //01: CH1 LEE TI1 Y CH2 LEE T12
    TIM5->CCMR1 |= (TIM_CCMR1_IC1F | TIM_CCMR1_IC2F);       //FILTRO DIGITAL MÁXIMO
    TIM5->CCER  |= (TIM_CCER_CC1E | TIM_CCER_CC2E);         //HABILITA LA CAPTURA DE LOS DOS CANALES
    TIM5->SMCR  |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1);       //011: MODOD ENCODER 3, CUENTA LOS FLANCOS DE TI1 Y TI2

    TIM5->CR1   |= TIM_CR1_CEN;     //ARRANCAR EL CONTADOR
}

//PB0 (SW DEL KY040) -> EXTI0, FLANCO DE BAJADA
static void exti0_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; //Reloj del GPIOB (PB0 queda como entrada, estado de reset)
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR0_0; //01: pull-up, en reposo lee 1 y al presionar baja a 0

	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; //Reloj del SYSCFG, el multiplexor de EXTI vive ahí
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PB; //La línea EXTI0 toma el pin 0 del puerto B

	EXTI->FTSR |= EXTI_FTSR_TR0; //Disparo por flanco de bajada
	EXTI->IMR |= EXTI_IMR_MR0; //Desenmascara la línea 0
	__NVIC_EnableIRQ(EXTI0_IRQn); //Habilita la interrupción en el NVIC
}

static void tim2_capture_Init(void)
{
	// Habilita GPIOB y configura PB10 con la función alternativa AF1 de TIM2_CH3.
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

	// PB10: FUNCIÓN ALTERNATIVA AF01 (TIM2_CH3)
	GPIOB->MODER &= ~GPIO_MODER_MODE10;
	GPIOB->MODER |= GPIO_MODER_MODE10_1;	//10: Alternate function mode
	GPIOB->AFR[1] &= ~GPIO_AFRH_AFRH2;
	GPIOB->AFR[1] |= GPIO_AFRH_AFRH2_0;		//0001: AF1

	// Habilita TIM2 y prepara una base de 1 MHz con contador de 32 bits.
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

	TIM2->CR1 = 0;
	TIM2->PSC = 15;				//Tick de 1 us
	TIM2->ARR  = 0xFFFFFFFF;		// TIM2 es de 32 bits
	TIM2->CNT = 0; //Inicia la cuenta desde cero

	TIM2->CCMR2 &= ~(TIM_CCMR2_CC3S | TIM_CCMR2_IC3PSC | TIM_CCMR2_IC3F);	//Limpia selección de entrada, prescaler y filtro de captura de CH3
	TIM2->CCMR2 |= TIM_CCMR2_CC3S_0;	//01: CH3 LEE TI3

	// Selecciona entrada directa TI3 y flanco ascendente; se dejan filtro y prescaler de captura en cero.
	TIM2->CCER &= ~(TIM_CCER_CC3P | TIM_CCER_CC3NP | TIM_CCER_CC3E); //Configura polaridad ascendente y deshabilita CH3 mientras se ajusta
	TIM2->CCER  |= TIM_CCER_CC3E;		//1: Capture enabled
	TIM2->SR = 0; //Limpia banderas pendientes antes de habilitar la interrupción
	TIM2->DIER |= TIM_DIER_CC3IE; //Solicita interrupción al capturar en el canal 3
	NVIC_ClearPendingIRQ(TIM2_IRQn); //Descarta una solicitud antigua en el NVIC
	NVIC_EnableIRQ(TIM2_IRQn); //Habilita la interrupción de TIM2 en el NVIC

	TIM2->CR1 |= TIM_CR1_CEN;			//Inicia el contador libre que proporciona las marcas de tiempo
}

/* ======= ISR: atienden el hardware y dejan datos/banderas para la FSM ======= */

//Cada 1 ms avanza el antirebote y el conteo del blinky; cada 125 ms deja la bandera para la FSM.
void TIM4_IRQHandler(void)
{
	static uint16_t blink_count = 0; //static: conserva su valor entre interrupciones

	TIM4->SR &= ~TIM_SR_UIF; //Baja la bandera (escribiendo 0) para que la interrupción se pueda repetir

	// Antirebote no bloqueante del pulsador. EXTI0 está enmascarada desde el primer flanco.
	if (button_debounce_ms > 0)
	{
		if (button_wait_release)
		{
			// Exige que el botón permanezca liberado durante BUTTON_DEBOUNCE_MS.
			if ((GPIOB->IDR & GPIO_IDR_ID0) != 0)
			{
				button_debounce_ms--; //Cuenta el tiempo desde el flanco inicial de EXTI0 //Acumula un milisegundo estable de liberación
			}
			else
			{
				button_debounce_ms = BUTTON_DEBOUNCE_MS; //Reinicia el tiempo si vuelve a estar presionado
			}

			if (button_debounce_ms == 0)
			{
				button_wait_release = 0; //Terminó la comprobación de que el pulsador quedó liberado
				EXTI->PR = EXTI_PR_PR0; //Descarta cualquier pendiente generado durante el rebote
				EXTI->IMR |= EXTI_IMR_MR0; //Rearma EXTI0 para la siguiente pulsación
			}
		}
		else
		{
			button_debounce_ms--;
			if (button_debounce_ms == 0)
			{
				if ((GPIOB->IDR & GPIO_IDR_ID0) == 0)
				{
					button_flag = 1; //Una pulsación validada; la FSM la procesa una sola vez
					button_wait_release = 1; //Tras validar, espera una liberación estable
					button_debounce_ms = BUTTON_DEBOUNCE_MS; //Inicia la validación del tiempo de liberación
				}
				else
				{
					// El flanco fue un rebote: rearma solo después de comprobar que sigue liberado.
					button_wait_release = 1; //No hubo pulsación válida; espera liberación para rearmar
					button_debounce_ms = BUTTON_DEBOUNCE_MS; //Exige que la liberación permanezca estable
				}
			}
		}
	}

	blink_count++;
	if (blink_count >= blink_half_period_ms)
	{
		blink_count = 0; //Reinicia el conteo para el siguiente intervalo de 125 ms
		blink_flag = 1; //La FSM consume esta bandera y la convierte en EV_BLINKY_TICK
	}
}

//Pulsador por EXTI0: confirma la pulsación y la liberación con el tick de 1 ms de TIM4.
void EXTI0_IRQHandler(void)
{
	EXTI->PR = EXTI_PR_PR0; //Se baja escribiendo 1 (en EXTI->PR se limpia con 1, no con 0)
	EXTI->IMR &= ~EXTI_IMR_MR0; //Enmascara nuevos flancos durante el antirebote
	button_wait_release = 0; //Comienza la validación de la pulsación
	button_debounce_ms = BUTTON_DEBOUNCE_MS; //Inicia la validación medida por TIM4
}

// Calcula el período entre marcas consecutivas; la primera captura inicializa la referencia.
void TIM2_IRQHandler(void)
{
	// Atiende la captura de CH3; leer CCR3 obtiene la marca temporal capturada.
	if (TIM2->SR & TIM_SR_CC3IF)
	{
		uint32_t captura = TIM2->CCR3; //Lectura CCR3 reconoce la captura pendiente

		if (captura_previa_valida)
		{
			// Aritmética unsigned: la resta también resuelve un desbordamiento del contador.
			captura_pendiente_ticks = captura - captura_anterior;
			captura_flag = 1; //Avisa a la FSM que hay un período nuevo disponible
		}
		else
		{
			captura_previa_valida = 1; //La primera marca establece el punto de referencia
		}

		captura_anterior = captura; //Guarda esta marca para calcular el período siguiente
	}
}
