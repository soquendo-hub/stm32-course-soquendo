NUCLEO-F411RE · Drivers HAL · VS Code y CMake

# UART + DMA + Comandos

Una guía paso a paso desde un LED parpadeante hasta una interfaz de comandos serial robusta: sondeo (polling), interrupciones, DMA con la línea IDLE, buffers ping-pong y un analizador de comandos.

Borrador 1.0, 2026-11-24. Etapas 0 a 5 verificadas en hardware.

La respuesta “OK” tal como sale por el pin TX: cada byte es un bit de inicio, ocho bits de datos enviados con el bit menos significativo primero, y un bit de parada.

> **Nota de traducción.** El código, los nombres de archivos, funciones y variables, y los textos literales que la placa imprime o espera recibir (por ejemplo `commands: help, info, ...` o `error: unknown command`) se dejan en inglés, porque forman parte del programa y deben coincidir exactamente con lo que verás en la terminal. Los comentarios dentro de los bloques de código sí están traducidos.

Contenido

1. [Cómo usar esta guía](#como-usar-esta-guia)
2. [Etapa 0 — Preparación](#etapa-0-preparacion)
   - [0.A — Compilar el driver UART](#pc-0a)
   - [0.B — Añadir el selector de aplicación](#pc-0b)
   - [0.C — Mover el heartbeat a módulos compartidos](#pc-0c)
   - [0.D — Preparar la terminal serial](#pc-0d)
3. [Etapa 1 — Sondeo (polling)](#etapa-1-sondeo)
   - [1.A — Añadir la etapa al build](#pc-1a)
   - [1.B — Inicializar USART2 e imprimir un banner](#pc-1b)
   - [1.C — Eco de teclas e impresión de un tick](#pc-1c)
4. [Etapa 2 — Interrupción y primeros comandos](#etapa-2-interrupcion-y-primeros-comandos)
   - [2.A — Añadir la etapa al build](#pc-2a)
   - [2.B — Recibir por interrupción en un buffer circular](#pc-2b)
   - [2.C — Ensamblar líneas](#pc-2c)
   - [2.D — Analizar y ejecutar comandos](#pc-2d)
5. [Etapa 3 — DMA y la línea IDLE](#etapa-3-dma-y-la-linea-idle)
   - [3.A — Añadir la etapa al build](#pc-3a)
   - [3.B — Recibir por DMA hasta que la línea quede inactiva](#pc-3b)
   - [3.C — Ráfagas largas y recuperación de errores](#pc-3c)
6. [Etapa 4 — Buffers ping-pong, transmisión por DMA y comandos](#etapa-4-buffers-ping-pong-transmision-por-dma-y-comandos)
   - [4.A — Añadir la etapa al build](#pc-4a)
   - [4.B — Dos bloques para la recepción](#pc-4b)
   - [4.C — Transmitir por DMA](#pc-4c)
   - [4.D — Líneas y comandos otra vez](#pc-4d)
7. [Etapa 5 — Añade tu propio comando: pwm](#etapa-5-anade-tu-propio-comando-pwm)
   - [5.A — Enseñarle al analizador una palabra nueva](#pc-5a)
   - [5.B — Añadir el módulo PWM](#pc-5b)
   - [5.C — Conectar el comando con la salida](#pc-5c)
8. [Cómo encajan las piezas](#como-encajan-las-piezas)

<a id="como-usar-esta-guia"></a>
## Cómo usar esta guía

Esta guía lleva un proyecto blinky funcional de la NUCLEO-F411RE hasta una interfaz de comandos UART robusta en cuatro etapas. El proyecto tiene un único `main.c` fijo; cada etapa es su propio archivo de aplicación (`app.c`), y CMake decide cuál se compila.

**Punto de partida.** Ya tienes un proyecto con VS Code + CMake (extensión STM32, sin STM32CubeMX) que hace parpadear LD2 (PA5) desde una interrupción de temporizador mediante `HAL_TIM_PeriodElapsedCallback`. Sabes poner en marcha un periférico con la HAL: habilitar sus relojes, configurar sus pines y su NVIC, llenar un handle y llamar a `HAL_xxx_Init`.

**Manual de referencia.** Cada función de la HAL se cita con respecto al UM1725 recortado para la F411 (capítulo 70 UART, capítulo 21 DMA, capítulo 31 GPIO, capítulo 11 CORTEX/NVIC). Lee la sección citada cuando un paso use una función por primera vez.

| Etapa | Valor de `APP` | Qué añade | Problema que deja al descubierto |
| --- | --- | --- | --- |
| 0 | `blinky` | Driver UART en el build, selector de aplicación en CMake, módulo heartbeat compartido | Ninguno: el comportamiento no cambia |
| 1 | `polling` | `HAL_UART_Transmit` / `HAL_UART_Receive` | La CPU queda bloqueada mientras espera |
| 2 | `interrupt` | Interrupción de RX por byte, ensamblado de líneas, analizador de comandos | Una interrupción por byte; un error de recepción detiene RX en silencio |
| 3 | `dma_idle` | Recepción por DMA terminada por la línea IDLE, `HAL_UART_MspInit` | Un buffer se sobrescribe mientras se procesa; las ráfagas largas se parten |
| 4 | `pingpong` | Dos buffers de RX, buzón protegido, transmisión por DMA | Ninguno: este es el diseño final |
| 5 | `pingpong` | Un comando nuevo, `pwm`, y un módulo PWM | Ninguno: muestra cómo extender el diseño |

**Plantilla de cada etapa.** Todas las etapas siguen el mismo orden: Objetivo, Conceptos nuevos, Puntos de control, Rómpelo, Preguntas de repaso. Un punto de control tiene como máximo tres pasos y termina con una prueba en la placa. No empieces el siguiente punto de control hasta que el actual pase.

**Escribe el código a mano.** Desde la Etapa 1 en adelante, los listados del `app.c` de cada etapa son imágenes, no texto, así que no se pueden copiar y pegar. Escribirlos es parte del ejercicio: lees cada línea, y las quejas del compilador por tus errores de tipeo te enseñan qué necesita cada línea.

**La regla del heartbeat.** LD2 es el latido del sistema y ningún comando lo apaga jamás. Si LD2 deja de parpadear, la CPU está atascada (HardFault, o una interrupción que nunca retorna). Si LD2 sigue parpadeando pero la placa no responde, la CPU está bien y el camino de la UART está roto. Usa esta regla cada vez que algo falle.

Etapa 0

<a id="etapa-0-preparacion"></a>
## Preparación

La Etapa 0 cambia el build, no el comportamiento. Al final corre el mismo blinky, pero ahora `main.c` es fijo, el comportamiento del programa viene de un archivo de aplicación elegido por un selector de CMake, el driver UART está compilado, y el heartbeat vive en un módulo compartido que reutilizan todas las etapas siguientes.

### Objetivo

- El driver HAL UART está compilado y enlazado.
- Un `main.c` fijo y un archivo de aplicación por etapa; la variable de CMake `APP` selecciona qué aplicación se construye.
- El heartbeat (TIM2 + LD2) y la configuración del reloj viven en `Core/common/`.
- Una terminal serial en el PC está lista para hablar con la placa.

Estructura del proyecto al final de la Etapa 0. Cada etapa posterior añade su propia carpeta `app_N_<nombre>/` junto a `app_0_blinky/`.

```
<project>/
├── CMakeLists.txt              ← selector APP, fuentes HAL (tu archivo)
├── cmake/                      ← generado por la extensión: nunca lo edites
├── Core/
│   ├── Src/main.c              ← main() fijo: HAL, reloj, heartbeat, app
│   ├── common/
│   │   ├── app.h               ← app_init(), app_loop()
│   │   ├── system.c/.h         ← SystemClock_Config(), Error_Handler()
│   │   └── heartbeat.c/.h      ← TIM2 + LD2 (PA5), su IRQ y su callback
│   └── app_0_blinky/app.c      ← Etapa 0: todavía sin UART
└── archivo de startup, linker script, stm32f4xx_hal_conf.h, stm32f4xx_it.c (sin cambios)
```

Mantén las carpetas `app_N_<nombre>/` fuera de `Core/Src/`. Si el archivo regenerado llegara a tomar todos los `.c` de esa carpeta, todas las aplicaciones se compilarían juntas y el enlazador reportaría `multiple definition of app_init`.

**Por qué un solo `main` y muchas apps.** El asistente del proyecto lista `Core/Src/main.c` en un archivo CMake que la extensión STM32 regenera, así que cualquier edición que hagas a esa lista se sobrescribe. Por eso nunca movemos ni quitamos `main.c` de la lista. En cambio, `main()` siempre hace el mismo arranque (HAL, reloj, heartbeat) y luego llama a dos funciones, `app_init()` y `app_loop()`. Cada etapa aporta su propia versión de esas dos funciones en su propio `app.c`, y el selector en `CMakeLists.txt`, que es tu archivo, decide qué `app.c` se compila.

### Conceptos nuevos

**USART2 en la NUCLEO-F411RE.** USART2 TX es PA2 y RX es PA3, ambos en la función alternativa 7 (AF7). En la Nucleo estos dos pines están conectados al ST-LINK de la propia placa, que aparece en el PC como un puerto COM virtual USB (`/dev/ttyACM0` en Linux). El cable USB es todo lo que necesitas. Por defecto PA2 y PA3 no están conectados a los pines Arduino D1/D0; consulta el manual de la placa (UM1724, puentes de soldadura) antes de usar esos conectores.

**La trama UART (8N1).** Cada byte viaja como 1 bit de inicio (bajo), 8 bits de datos (el menos significativo primero), sin paridad y 1 bit de parada (alto). Son 10 tiempos de bit por byte. Entre bytes la línea descansa en alto (inactiva). La figura muestra la letra `A` (0x41 = 0100 0001).

A 115200 baudios un bit dura 8,68 µs, un byte dura 86,8 µs, y la tasa máxima es de 11 520 bytes/s. Ten presentes los 86,8 µs: en la Etapa 3 el evento IDLE se dispara un tiempo de trama después del último byte.

**Quién fija la tasa de baudios.** `HAL_UART_Init` calcula el registro de baudios (`USART_BRR`) a partir del reloj APB1 que lee con `HAL_RCC_GetPCLK1Freq()`, porque USART2 está en APB1. Por eso `SystemClock_Config()` debe ser correcta, y APB1 no debe superar los 50 MHz. Un reloj equivocado se manifiesta como caracteres basura en la terminal.

**RS-232 no son los pines de la UART.** Los pines del MCU usan lógica de 0 V / 3,3 V. RS-232 es un estándar eléctrico con niveles invertidos y bipolares: el 1 lógico es de −3 a −15 V y el 0 lógico es de +3 a +15 V. Nunca conectes un DB9 de PC directamente a PA2/PA3. Usa un transceptor que funcione a 3,3 V, como el MAX3232 (el clásico MAX232 necesita 5 V de alimentación). Esta guía nunca lo necesita: el ST-LINK lleva la UART por USB.

**Un driver HAL = un define + un archivo `.c`.** En `stm32f4xx_hal_conf.h` habilitas un driver escribiendo `#define HAL_xxx_MODULE_ENABLED`, donde `xxx` es el nombre del módulo (`UART`, `TIM`, `DMA`...). Cada define tiene dos efectos:

1. El encabezado de configuración incluye `stm32f4xx_hal_uart.h` solo cuando `HAL_UART_MODULE_ENABLED` está definido. Sin él, `UART_HandleTypeDef` es desconocido y el compilador se detiene.
2. Todo el cuerpo de `stm32f4xx_hal_uart.c` está envuelto en `#ifdef HAL_UART_MODULE_ENABLED`. El archivo puede compilarse y aun así quedar vacío, y entonces el enlazador reporta `undefined reference to HAL_UART_Init`. Obtienes el mismo error de enlace si el define está puesto pero el archivo `.c` no está en CMake.

El define y el archivo fuente siempre van juntos.

**Funciones débiles (weak).** La HAL define sus callbacks (`HAL_TIM_PeriodElapsedCallback`, `HAL_UART_RxCpltCallback`, ...) como funciones `__weak` que no hacen nada, y el archivo de startup hace lo mismo con cada manejador de interrupción (`TIM2_IRQHandler`, `USART2_IRQHandler`, ...). Cuando tu código define una función con el mismo nombre, el enlazador usa la tuya; no hace falta registrar nada (`USE_HAL_UART_REGISTER_CALLBACKS` se queda en 0). La trampa: cada uno de estos nombres puede definirse una sola vez en todo el programa. Por eso cada etapa mantiene su manejador de interrupción de UART y sus callbacks dentro de su propio `app.c`, y por eso el selector compila exactamente un `app.c` a la vez. La HAL tiene otro tipo de gancho débil, `HAL_xxx_MspInit`; la Etapa 3 lo introduce.

<a id="pc-0a"></a>
### Punto de control 0.A — Compilar el driver UART

1. Abre `stm32f4xx_hal_conf.h`. Tu proyecto habilita solo los módulos que usa, cada uno con su propia línea siguiendo el patrón `#define HAL_xxx_MODULE_ENABLED`, donde `xxx` es el nombre del módulo. Escribe la nueva línea para la UART junto a las existentes:

   ```
   #define HAL_UART_MODULE_ENABLED     /* nuevo: driver UART */
   ```

   Mientras estás ahí, confirma que las líneas de `DMA`, `GPIO`, `RCC`, `CORTEX` y `TIM` ya están escritas. El blinky las necesita; DMA es necesario porque el handle de TIM contiene punteros a handles de DMA.
2. En `CMakeLists.txt`, añade la fuente del driver UART junto a los archivos HAL que ya compilas:

   ```
   ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
   ```
3. Recompila y flashea.

Verifica en la placa

el build termina sin errores ni advertencias nuevos, y LD2 sigue parpadeando. Todavía no corre código de UART; este punto de control solo demuestra que el driver compila y enlaza.

<a id="pc-0b"></a>
### Punto de control 0.B — Añadir el selector de aplicación

1. Crea dos archivos nuevos.

   **El archivo `Core/common/app.h`** declara las dos funciones que toda aplicación debe proveer:

   ```
   /* Core/common/app.h — las dos funciones que cada etapa provee */
   #ifndef APP_H
   #define APP_H

   void app_init(void);   /* se llama una vez, después de que el reloj y el heartbeat están corriendo */
   void app_loop(void);   /* se llama para siempre desde el bucle while (1) de main() */

   #endif
   ```

   **El archivo `Core/app_0_blinky/app.c`** es la primera aplicación. Por ahora ambas funciones están vacías:

   ```
   /* Core/app_0_blinky/app.c — Etapa 0: todavía sin UART */
   #include "app.h"

   void app_init(void)
   {
       /* nada todavía */
   }

   void app_loop(void)
   {
       /* nada: el heartbeat corre desde la interrupción de TIM2 */
   }
   ```
2. Añade el selector a `CMakeLists.txt`, después de `add_executable(...)`. No toques el archivo CMake regenerado: `main.c` sigue listado allí.

   ```
   # ---- Selector de aplicación -------------------------------------------
   # Un main.c fijo, varias aplicaciones: APP decide qué app.c se compila.
   # Cada etapa nueva añade su nombre a la lista STRINGS y una línea APP_DIR_<nombre>.
   set(APP "blinky" CACHE STRING "Application to build")
   set_property(CACHE APP PROPERTY STRINGS blinky)

   set(APP_DIR_blinky ${CMAKE_SOURCE_DIR}/Core/app_0_blinky)

   if(NOT DEFINED APP_DIR_${APP})
       message(FATAL_ERROR "Unknown APP='${APP}': there is no APP_DIR_${APP} line in CMakeLists.txt")
   endif()

   target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Core/common)
   target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${APP_DIR_${APP}}/app.c)
   message(STATUS ">>> Building APP = ${APP}")
   ```
3. Selecciona la aplicación en `.vscode/settings.json`. La extensión ya creó allí una lista `"cmake.configureArgs"`; añade `-DAPP=blinky` como segundo elemento (cuidado con la coma). Luego configura desde VS Code con `Ctrl+Shift+P` → **CMake: Configure**, y compila con el botón de compilación habitual.

   ```
   "cmake.configureArgs": [
       "-DCMAKE_COMMAND=cube-cmake",
       "-DAPP=blinky"
   ],
   ```

**Por qué `configureArgs` y no `configureSettings`.** El proyecto se configura mediante un preset de CMake (`Debug`). En ese modo VS Code añade los elementos de `"cmake.configureArgs"` a la línea de comandos de CMake, pero no pasa las entradas de `"cmake.configureSettings"`, así que un `APP` escrito ahí se ignora en silencio y se sigue construyendo la aplicación anterior. Siempre puedes comprobar qué llegó a CMake: la línea `[proc] Executing command:` en el panel de salida de CMake debe contener `-DAPP=...`.

**Por qué configurar desde VS Code y no desde una terminal.** La extensión STM32 instala el compilador en su propia carpeta (`~/.local/share/stm32cube/bundles/`) y ejecuta CMake mediante su envoltorio, `cube-cmake`, que añade esa carpeta al path. Una terminal normal que ejecute `cmake --preset Debug` no encuentra el compilador y se detiene con `CMAKE_C_COMPILER: arm-none-eabi-gcc is not a full path and was not found in the PATH`. Si eso ocurre, borra la carpeta de build a medio hacer (`rm -rf build/Debug`) y ejecuta **CMake: Delete Cache and Reconfigure** desde VS Code.

**Por qué una variable de caché.** CMake guarda `APP` en `build/Debug/CMakeCache.txt` y la recuerda entre compilaciones. VS Code pasa `-DAPP=...` desde `settings.json` cada vez que configura, y ese valor reemplaza al de la caché (el `"blinky"` de `set(APP "blinky" CACHE ...)` es solo el valor inicial). Para cambiar de aplicación, cambia el elemento `-DAPP=` en `settings.json` y ejecuta **CMake: Configure** antes de compilar; una compilación simple sigue compilando la aplicación anterior. La línea `>>> Building APP = ...` en el panel de salida de CMake te dice qué aplicación estás por flashear, así que léela siempre.

Verifica en la placa

el panel de salida de CMake muestra `>>> Building APP = blinky`, y el build tiene éxito. `main.c` todavía no llama a la aplicación, así que tras flashear LD2 parpadea exactamente como antes. Luego cambia el elemento a `-DAPP=foo` y configura de nuevo: la línea `Executing command` muestra `-DAPP=foo`, y la configuración debe detenerse con el mensaje `Unknown APP='foo'`. Vuelve a cambiarlo a `-DAPP=blinky` y configura otra vez antes de continuar.

<a id="pc-0c"></a>
### Punto de control 0.C — Mover el heartbeat a módulos compartidos

1. Crea dos archivos nuevos, `Core/common/system.h` y `Core/common/system.c`. Mueve tu `SystemClock_Config()` existente de `main.c` a `system.c` sin cambios, y escribe el nuevo `Error_Handler()` que se muestra abajo. Si tu función de reloj tiene otro nombre, conserva tu nombre y ajusta `system.h`.

   **Archivo `Core/common/system.h`:**

   ```
   /* Core/common/system.h */
   #ifndef SYSTEM_H
   #define SYSTEM_H

   void SystemClock_Config(void);
   void Error_Handler(void);   /* deshabilita las interrupciones y se queda en bucle infinito */

   #endif
   ```

   **Archivo `Core/common/system.c`:**

   ```
   /* Core/common/system.c */
   #include "stm32f4xx_hal.h"
   #include "system.h"

   void SystemClock_Config(void)
   {
       /* tu configuración de reloj existente, movida aquí sin cambios */
   }

   void Error_Handler(void)
   {
       __disable_irq();     /* detiene toda interrupción: el heartbeat se congela, un "CPU atascada" visible */
       while (1) {
           /* quédate aquí; el depurador muestra dónde terminamos */
       }
   }
   ```

   **Por qué un `Error_Handler()` nuevo.** A partir de ahora, cada llamada a la HAL que devuelve un estado (`HAL_TIM_Base_Init`, `HAL_UART_Init`, ...) se comprueba, y un fallo debe terminar en un lugar visible. `Error_Handler()` detiene todas las interrupciones, así que LD2 se congela: la firma de “CPU atascada” de la regla del heartbeat. En el depurador, la pila de llamadas muestra qué llamada falló.
2. Crea `Core/common/heartbeat.h` y `Core/common/heartbeat.c`, y mueve allí tu código del temporizador. La versión de abajo usa TIM2; si tu blinky usa otro TIMx, conserva el tuyo y adopta solo la interfaz. Borra las funciones movidas (el manejador IRQ del TIM, `HAL_TIM_PeriodElapsedCallback`, y `HAL_TIM_Base_MspInit` si tienes una) de sus archivos anteriores, de lo contrario el enlazador reporta `multiple definition`.

   **Archivo `Core/common/heartbeat.h`:**

   ```
   /* Core/common/heartbeat.h */
   #ifndef HEARTBEAT_H
   #define HEARTBEAT_H

   #include <stdbool.h>
   #include <stdint.h>

   #define HEARTBEAT_MIN_MS    50U
   #define HEARTBEAT_MAX_MS  5000U

   void     heartbeat_init(uint32_t toggle_ms);
   bool     heartbeat_set_period(uint32_t toggle_ms);  /* lo usa el comando "blink" en la Etapa 2 */
   uint32_t heartbeat_get_period(void);

   #endif
   ```

   **Archivo `Core/common/heartbeat.c`:**

   ```
   /* Core/common/heartbeat.c */
   #include "stm32f4xx_hal.h"
   #include "heartbeat.h"
   #include "system.h"

   #define HB_TIM       TIM2
   #define HB_TICK_HZ   10000U        /* el contador corre a 10 kHz: 1 tick = 0,1 ms */
   #define HB_LED_PORT  GPIOA
   #define HB_LED_PIN   GPIO_PIN_5    /* LD2 */

   static TIM_HandleTypeDef htim_hb;  /* static: solo este archivo puede tocarlo */
   static volatile uint32_t hb_period_ms;

   /* TIM2 está en APB1: su reloj es PCLK1, o 2 x PCLK1 cuando el prescaler de APB1 no es 1 */
   static uint32_t hb_timer_clock_hz(void)
   {
       uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
       if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) {
           return pclk1;
       }
       return 2U * pclk1;
   }

   void heartbeat_init(uint32_t toggle_ms)
   {
       GPIO_InitTypeDef gpio = {0};

       /* 1. Relojes: GPIOA para el LED, TIM2 para el temporizador */
       __HAL_RCC_GPIOA_CLK_ENABLE();
       __HAL_RCC_TIM2_CLK_ENABLE();

       /* 2. Pin del LED */
       gpio.Pin   = HB_LED_PIN;
       gpio.Mode  = GPIO_MODE_OUTPUT_PP;
       gpio.Pull  = GPIO_NOPULL;
       gpio.Speed = GPIO_SPEED_FREQ_LOW;
       HAL_GPIO_Init(HB_LED_PORT, &gpio);

       /* 3. Registros del temporizador */
       hb_period_ms = toggle_ms;
       htim_hb.Instance               = HB_TIM;
       htim_hb.Init.Prescaler         = (hb_timer_clock_hz() / HB_TICK_HZ) - 1U;
       htim_hb.Init.CounterMode       = TIM_COUNTERMODE_UP;
       htim_hb.Init.Period            = (toggle_ms * (HB_TICK_HZ / 1000U)) - 1U;
       htim_hb.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
       htim_hb.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
       if (HAL_TIM_Base_Init(&htim_hb) != HAL_OK) {
           Error_Handler();
       }

       /* 4. Interrupción: primero el NVIC, luego arranca el temporizador con su interrupción de actualización */
       HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
       HAL_NVIC_EnableIRQ(TIM2_IRQn);
       if (HAL_TIM_Base_Start_IT(&htim_hb) != HAL_OK) {
           Error_Handler();
       }
   }

   bool heartbeat_set_period(uint32_t toggle_ms)
   {
       if ((toggle_ms < HEARTBEAT_MIN_MS) || (toggle_ms > HEARTBEAT_MAX_MS)) {
           return false;                  /* rechazar: el heartbeat debe seguir siendo visible */
       }
       hb_period_ms = toggle_ms;
       __HAL_TIM_SET_AUTORELOAD(&htim_hb, (toggle_ms * (HB_TICK_HZ / 1000U)) - 1U);
       return true;
   }

   uint32_t heartbeat_get_period(void)
   {
       return hb_period_ms;
   }

   /* ---- Manejador de interrupción y callback de la HAL (funciones débiles sobrescritas aquí) ---- */
   void TIM2_IRQHandler(void)
   {
       HAL_TIM_IRQHandler(&htim_hb);
   }

   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
   {
       if (htim->Instance == HB_TIM) {
           HAL_GPIO_TogglePin(HB_LED_PORT, HB_LED_PIN);
       }
   }
   ```
3. Reemplaza el contenido de `Core/Src/main.c` con la versión fija de abajo. El archivo se queda donde lo puso el asistente, y sigue listado en el archivo CMake regenerado; solo cambia su contenido. Todo lo que contenía el `main.c` anterior se movió a `system.c` y `heartbeat.c`. Luego añade las dos fuentes comunes a `CMakeLists.txt`, junto al selector.

   **Archivo `Core/Src/main.c`** (reemplaza todo su contenido):

   ```
   /* Core/Src/main.c — igual para todas las etapas; no se vuelve a editar */
   #include "stm32f4xx_hal.h"
   #include "system.h"
   #include "heartbeat.h"
   #include "app.h"

   int main(void)
   {
       HAL_Init();
       SystemClock_Config();
       heartbeat_init(250U);   /* cambia de estado cada 250 ms = parpadeo a 2 Hz */
       app_init();             /* provista por la etapa seleccionada */

       while (1) {
           app_loop();         /* provista por la etapa seleccionada */
       }
   }
   ```

   **En `CMakeLists.txt`**, después del selector:

   ```
   target_sources(${CMAKE_PROJECT_NAME} PRIVATE
       ${CMAKE_SOURCE_DIR}/Core/common/system.c
       ${CMAKE_SOURCE_DIR}/Core/common/heartbeat.c
   )
   ```

**Por qué el orden dentro de `heartbeat_init()`.** Los registros de un periférico ignoran toda escritura mientras su reloj está apagado, así que los relojes van primero. El NVIC se habilita antes de arrancar el temporizador, para que la primera interrupción de actualización tenga adónde ir. Las Etapas 1 y 2 ponen en marcha USART2 exactamente en el mismo orden: relojes, pines, periférico, NVIC, arranque.

**Por qué el temporizador lee su propio reloj.** `hb_timer_clock_hz()` calcula el prescaler a partir del reloj APB1 real, así que la tasa de parpadeo se mantiene correcta si `SystemClock_Config()` cambia más adelante. El temporizador cuenta a 10 kHz, y `Period` convierte milisegundos en ticks.

**Por qué el preload del auto-reload está activado.** `heartbeat_set_period()` cambia `ARR` mientras el temporizador corre. Con el preload habilitado, el nuevo valor surte efecto solo en el siguiente evento de actualización. Sin él, acortar el periodo cuando el contador ya pasó el nuevo `ARR` hace que el contador corra hasta su máximo antes de dar la vuelta (2^32 ticks, unos 5 días para el TIM2 de 32 bits a 10 kHz, así que el LED simplemente se detiene).

Verifica en la placa

recompila (no hace falta reconfigurar, ya que `APP` no cambió) y flashea. LD2 parpadea a 2 Hz: unos 10 destellos en 5 segundos. Luego, en `Core/app_0_blinky/app.c`, añade `#include "stm32f4xx_hal.h"` y `#include "heartbeat.h"`, y pon `HAL_Delay(5000U); heartbeat_set_period(1000U);` dentro de `app_init()`: tras 5 segundos el parpadeo debe pasar a 0,5 Hz. Quita esas dos líneas después.

<a id="pc-0d"></a>
### Punto de control 0.D — Preparar la terminal serial

Sirve cualquier terminal serial: CoolTerm (gráfica, la que usamos en clase), minicom o picocom. Lo importante es que esté lista y configurada antes de la Etapa 1.

1. Conecta la Nucleo y ejecuta `ls /dev/ttyACM*`; deberías ver `/dev/ttyACM0`. Si más adelante la terminal reporta `Permission denied`, a tu usuario le falta pertenecer al grupo `dialout` (consulta la sección de udev de la guía de instalación).
2. Configura la terminal para `/dev/ttyACM0` a 115200 baudios, 8 bits de datos, sin paridad, 1 bit de parada (8N1), sin control de flujo, y conéctate. En CoolTerm estos ajustes están en **Options → Serial Port**. Una alternativa de línea de comandos es picocom (`sudo apt install picocom`, luego `picocom -b 115200 /dev/ttyACM0`; sal con `Ctrl-A` y luego `Ctrl-X`).
3. Averigua qué envía tu tecla Enter: un retorno de carro (`\r`, 0x0D), un salto de línea (`\n`, 0x0A) o ambos (`\r\n`). En CoolTerm es la opción **Enter Key Emulation** en **Options → Terminal**; picocom envía `\r`. Anota el ajuste de tu terminal: la Etapa 2 acepta los tres, pero los ejercicios de Rómpelo dependen de saberlo.

La mayoría de las terminales no muestran lo que escribes a menos que el eco local esté habilitado. No verás nada hasta que la placa empiece a hacer eco en la Etapa 1.

Verifica en la placa

la terminal se conecta a `/dev/ttyACM0` sin errores. Escribir no produce salida todavía, lo cual es correcto: la placa no tiene código de UART.

Rómpelo

En `Core/app_0_blinky/app.c`, añade `#include "stm32f4xx_hal.h"` y `#include "system.h"`, pon `HAL_Delay(3000U); Error_Handler();` dentro de `app_init()`, y flashea. Tras 3 segundos LD2 se congela, encendido o apagado. Esta es la firma de “CPU atascada” de la regla del heartbeat: `Error_Handler()` deshabilita las interrupciones, así que la interrupción de TIM2 ya no puede cambiar el estado del LED. Recuerda esta imagen; es como distinguirás una CPU atascada de una UART rota en las siguientes etapas. Quita las dos líneas cuando termines.

### Preguntas de repaso

1. ¿Qué error obtienes, y en qué fase del build (compilación o enlace), si `HAL_UART_MODULE_ENABLED` está definido pero `stm32f4xx_hal_uart.c` falta en CMake? ¿Y en el caso inverso?
2. ¿Por qué cada etapa mantiene su propio `USART2_IRQHandler` y sus callbacks de UART dentro de su `app.c` en lugar de compartirlos en `Core/common/`?
3. ¿Por qué nunca movemos `Core/Src/main.c` ni lo quitamos del archivo CMake que genera la extensión? ¿Qué cambia en su lugar el selector?
4. A 115200 baudios con trama 8N1, ¿cuánto tarda un byte y cuál es el número máximo de bytes por segundo?
5. ¿Por qué PA2 nunca debe conectarse directamente al pin 3 de un puerto serial DB9 de PC?
6. Cambiaste `APP` en `.vscode/settings.json` pero la aplicación anterior sigue corriendo tras flashear. ¿Qué olvidaste?
7. LD2 deja de parpadear y la terminal queda en silencio. ¿Qué te dice la regla del heartbeat? ¿Y si LD2 sigue parpadeando?

Etapa 1

<a id="etapa-1-sondeo"></a>
## Sondeo (polling)

La Etapa 1 hace que la placa hable: envía texto con `HAL_UART_Transmit` y lee teclas con `HAL_UART_Receive`. Ambas llamadas son bloqueantes, así que la CPU espera dentro de ellas, y esta etapa muestra exactamente qué cuesta esa espera.

### Objetivo

- USART2 se inicializa a mano, en el mismo orden que el heartbeat: relojes, pines, periférico.
- La placa imprime un banner, una línea `tick` cada segundo, y una descripción de cada tecla que presionas.
- Puedes ver, en la placa, las dos debilidades del sondeo: el bucle se congela mientras espera, y se pierden bytes mientras está ocupado.

Estructura del proyecto al final de la Etapa 1 (nuevo: `app_1_polling/`):

```
<project>/
├── CMakeLists.txt              ← el selector ahora lista: blinky polling
├── Core/
│   ├── Src/main.c              ← sin cambios
│   ├── common/                 ← sin cambios
│   ├── app_0_blinky/app.c
│   └── app_1_polling/app.c     ← Etapa 1
└── ...
```

### Conceptos nuevos

**El handle de la UART.** Igual que `TIM_HandleTypeDef` para el heartbeat, un `UART_HandleTypeDef` contiene todo lo que la HAL necesita: `Instance` (qué periférico, `USART2`) e `Init` (tasa de baudios, longitud de palabra, bits de parada, paridad, dirección, control de flujo, sobremuestreo). `HAL_UART_Init()` convierte `Init` en valores de registros (UM1725 cap. 70, `HAL_UART_Init`).

**`HAL_UART_Init` llama a `HAL_UART_MspInit`.** Justo antes de escribir los registros de la USART, `HAL_UART_Init()` llama a la función débil y vacía `HAL_UART_MspInit()`. No usamos ese gancho hasta la Etapa 3, así que los relojes y los pines ya deben estar configurados cuando se ejecuta `HAL_UART_Init()`. Es la misma regla que en `heartbeat_init()`: los relojes primero.

**Llamadas bloqueantes.** `HAL_UART_Transmit(&huart2, data, size, timeout)` retorna solo cuando todos los bytes salieron del registro de desplazamiento, o cuando pasaron `timeout` milisegundos. `HAL_UART_Receive(&huart2, buffer, size, timeout)` retorna cuando llegaron `size` bytes (`HAL_OK`) o cuando expira el timeout (`HAL_TIMEOUT`). El timeout se cuenta con `HAL_GetTick()`, así que necesita el SysTick que arrancó `HAL_Init()`. `HAL_MAX_DELAY` significa “esperar para siempre”.

**El receptor guarda un solo byte.** La USART tiene un único registro de datos de recepción. Si un segundo byte termina de llegar antes de que la CPU lea el primero, el hardware activa la bandera de overrun y el nuevo byte se pierde. Con sondeo, la CPU lee solo mientras está dentro de `HAL_UART_Receive`. Cada milisegundo que pasa en otra parte (imprimiendo, calculando) es una ventana en la que los bytes entrantes pueden descartarse.

### Notas del lenguaje C

Cuatro detalles de C aparecen en el código de esta etapa. Ninguno es específico de la STM32: los encontrarás en todos los programas de aquí en adelante.

**Constantes sin signo: `0U`, `100U`, `0x20U`.** Un número escrito solo, como `100`, es un `int`, un entero con signo. El sufijo `U` le dice al compilador que la constante es sin signo (`unsigned int`), y funciona igual en hexadecimal (`0x20U`). Casi todos los valores de este código son sin signo: contenido de registros, tamaños, contadores y tiempos en milisegundos (`uint8_t`, `uint16_t`, `uint32_t`). Cuando una expresión mezcla valores con y sin signo, C convierte silenciosamente uno de ellos, con resultados sorprendentes: `-1 < 1U` es falso, porque `-1` se convierte en el valor sin signo 4 294 967 295. Escribir `U` mantiene ambos lados de cada operación del mismo tipo, y estándares de codificación como MISRA C lo exigen. Sufijos relacionados son `L` (long) y `UL` (unsigned long).

**Casts: `(tipo)valor`.** Un tipo escrito entre paréntesis antes de un valor convierte el valor a ese tipo. Esta etapa usa tres:

- `(uint16_t)strlen(text)`: `strlen()` devuelve un `size_t`, pero `HAL_UART_Transmit()` recibe la longitud como `uint16_t`. El cast hace visible la conversión, y tú asumes la responsabilidad de que el valor quepa en 16 bits.
- `(const uint8_t *)text`: el texto es un puntero a `char`, y la HAL espera un puntero a bytes. Los bytes en memoria no cambian; solo cambia el tipo que ve el compilador.
- `(unsigned long)tick_count`: hace que el argumento coincida con la conversión `%lu` de `snprintf()` (ver abajo).

**`(void)` delante de una llamada.** `HAL_UART_Transmit()` devuelve un estado (`HAL_OK`, `HAL_TIMEOUT`, ...). Escribir `(void)HAL_UART_Transmit(...)` convierte ese resultado en “nada”. Le dice al lector, y a cualquier herramienta que revise el código, que el valor se ignora a propósito. Sin el cast la línea compila igual, pero nadie puede saber si la comprobación se olvidó. La regla 7 de “Power of 10” pide comprobar cada valor de retorno, o descartarlo explícitamente así. El mismo cast sobre un nombre, `(void)param;`, marca un parámetro de función que no se usa a propósito y silencia la advertencia “unused parameter” del compilador.

**Cómo construye `snprintf()` una línea de texto.** `snprintf(buffer, size, format, ...)` funciona como `printf()`, pero escribe en un arreglo de caracteres en lugar de una pantalla.

- `buffer` y `size` indican dónde escribir y cuántos bytes caben. Escribe como máximo `size - 1` caracteres y siempre añade el `'\0'` final. Pasa `sizeof buffer`, para que el tamaño nunca pueda estar mal.
- `format` se copia carácter por carácter, excepto las conversiones que empiezan con `%`. Cada conversión se reemplaza por el siguiente argumento, en orden.
- Devuelve cuántos caracteres necesita el texto completo, sin contar el `'\0'`. Un resultado igual o mayor que `size` significa que el texto se cortó.

| Conversión | Tipo del argumento | Ejemplo | Resultado |
| --- | --- | --- | --- |
| `%d` | `int` | `"%d", -5` | `-5` |
| `%u` | `unsigned int` | `"%u ms", 50U` | `50 ms` |
| `%lu` | `unsigned long` | `"%lu", (unsigned long)1234U` | `1234` |
| `%ld` | `long` | `"%ld", -7L` | `-7` |
| `%X` | `unsigned int`, en hexadecimal | `"0x%X", 255U` | `0xFF` |
| `%02X` | igual, al menos 2 dígitos, rellenado con `0` | `"0x%02X", 13U` | `0x0D` |
| `%c` | un carácter | `"'%c'", 'a'` | `'a'` |
| `%s` | una cadena C, impresa hasta su `'\0'` | `"[%s]", "hi"` | `[hi]` |
| `%%` | ninguno: imprime un `%` | `"100%%"` | `100%` |

El argumento debe coincidir con su conversión, porque `snprintf()` no puede comprobar los tipos mientras el programa corre. Un `uint32_t` es un `unsigned long` en la STM32 pero un `unsigned int` en un PC, por eso el código lo convierte con `(unsigned long)` y usa `%lu`, que es correcto en ambos. GCC advierte de muchos desajustes (`-Wformat`): lee esas advertencias.

`\r` y `\n` no son conversiones. Son secuencias de escape que el compilador convierte en un carácter cada una: retorno de carro (0x0D) y salto de línea (0x0A). Una terminal necesita ambos, `\r\n`, para empezar una nueva línea en el margen izquierdo.

Un límite que debes recordar: la pequeña biblioteca C que usan la mayoría de los proyectos STM32 (newlib-nano) omite `%f` para ahorrar flash, así que un `float` no imprime nada. Imprime los valores como enteros, por ejemplo milivoltios con `%lu`.

<a id="pc-1a"></a>
### Punto de control 1.A — Añadir la etapa al build

1. Crea la carpeta `Core/app_1_polling/` y copia allí `Core/app_0_blinky/app.c`, sin cambios. Por ahora es un marcador de posición.
2. En `CMakeLists.txt`, añade el nuevo nombre a la lista y una línea de directorio:

   ```
   set_property(CACHE APP PROPERTY STRINGS blinky polling)

   set(APP_DIR_blinky  ${CMAKE_SOURCE_DIR}/Core/app_0_blinky)
   set(APP_DIR_polling ${CMAKE_SOURCE_DIR}/Core/app_1_polling)
   ```
3. En `.vscode/settings.json`, cambia el elemento a `-DAPP=polling`, luego ejecuta **CMake: Configure** y compila.

Verifica en la placa

la salida de CMake muestra `>>> Building APP = polling`, el build tiene éxito, y LD2 parpadea a 2 Hz tras flashear. Todavía no pasa nada más.

<a id="pc-1b"></a>
### Punto de control 1.B — Inicializar USART2 e imprimir un banner

1. Reemplaza `Core/app_1_polling/app.c` con la versión de abajo. Lee `uart2_init()` de arriba abajo: sigue el mismo orden que `heartbeat_init()`.

   **Archivo `Core/app_1_polling/app.c`:**

   Imagen de Core/app_1_polling/app.c: escríbelo tú mismo

   Imagen del código: escríbelo tú mismo.
2. Compila y flashea, con la terminal conectada.
3. Presiona el botón negro RESET (B2) de la Nucleo.

Verifica en la placa

la terminal muestra `Stage 1: polling. Type something.` después de cada reset, y LD2 sigue parpadeando. Si en cambio ves caracteres basura, la tasa de baudios de la terminal y la de `Init.BaudRate` no coinciden, o `SystemClock_Config()` no es el reloj que crees que es.

<a id="pc-1c"></a>
### Punto de control 1.C — Eco de teclas e impresión de un tick

1. Reemplaza el `app_loop()` vacío con la versión de abajo, y añade los dos defines nuevos junto a `TX_TIMEOUT_MS`.

   **En `Core/app_1_polling/app.c`:**

   Imagen del nuevo app_loop() en Core/app_1_polling/app.c: escríbelo tú mismo

   Imagen del código: escríbelo tú mismo.
2. Compila y flashea.
3. Escribe algunas teclas despacio, luego presiona Enter.

Verifica en la placa

aparece una línea `tick N at T ms` cada segundo, con `T` creciendo en unos 1000. Cada tecla que escribes produce una línea `rx:`, por ejemplo `rx: 'a' 0x61`. Enter se muestra como `rx: '.' 0x0D` (y `0x0A` si tu terminal envía `\r\n`): este es el final de línea que anotaste en el Punto de control 0.D.

**Por qué imprimir la hora en la línea tick.** La hora es la evidencia para el ejercicio Rómpelo: cuando el bucle está bloqueado, las líneas tick se detienen, y la siguiente muestra un salto en `T` mucho mayor que 1000 ms.

**Por qué `snprintf` y no `sprintf`.** `snprintf` recibe el tamaño de `msg` y nunca escribe más allá. `sprintf` escribe tantos caracteres como produzca el formato, y un valor largo sobrescribe en silencio lo que esté después de `msg` en la pila.

Rómpelo

Dos experimentos, uno tras otro.

Primero, cambia `RX_TIMEOUT_MS` a `HAL_MAX_DELAY` y flashea. Las líneas tick se detienen: `app_loop()` ahora espera para siempre dentro de `HAL_UART_Receive` hasta que llegue una tecla. LD2 sigue parpadeando, porque el heartbeat corre desde su propia interrupción: la regla del heartbeat dice que la CPU está viva y solo el bucle está atascado. Presiona una tecla y mira la siguiente línea tick: `T` ha saltado el tiempo que esperaste. Devuelve `RX_TIMEOUT_MS` a `10U`.

Segundo, copia el texto `0123456789` y pégalo en la ventana de la terminal de una sola vez. Cuenta las líneas `rx:`: solo unos pocos de los 10 caracteres regresan. Cada línea `rx:` tiene 14 caracteres, lo que toma unos 1,2 ms en transmitirse a 115200 baudios, mientras que un nuevo carácter pegado llega cada 0,087 ms. Durante cada transmisión el receptor sufre overrun y se descartan caracteres. La Etapa 2 lo arregla con una interrupción que atrapa cada byte, sin importar lo que esté haciendo el bucle principal.

### Preguntas de repaso

1. En `uart2_init()`, ¿por qué `__HAL_RCC_USART2_CLK_ENABLE()` debe ir antes de `HAL_UART_Init()`? ¿Qué pasaría si el orden se invirtiera?
2. `HAL_UART_Init()` llama a `HAL_UART_MspInit()`. ¿Por qué la UART sigue funcionando en esta etapa, aunque nunca escribimos esa función?
3. ¿Qué devuelve `HAL_UART_Receive` cuando no se presiona ninguna tecla durante `RX_TIMEOUT_MS`, y cómo usa `app_loop()` ese valor?
4. Con `RX_TIMEOUT_MS = HAL_MAX_DELAY`, las líneas tick se detienen pero LD2 sigue parpadeando. Explica ambas observaciones.
5. En el experimento de pegado, ¿aproximadamente cuántos caracteres pueden llegar mientras se transmite una línea `rx:`? Muestra el cálculo.
6. ¿Por qué `huart2` se declara `static`? Si dos archivos `app.c` se compilaran juntos por error, ¿qué nombres chocarían al enlazar y cuáles no?
