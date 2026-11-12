# Tutorial HAL Blinky — Semana 08
## Primer proyecto con las librerías HAL de STM32F4xx (sin CubeMX)

Este tutorial te guía en la creación de tu primer proyecto basado en HAL desde cero,
sin usar CubeMX. Cada paso es explícito — nada se genera automáticamente.
Al final tendrás un LED parpadeando en PA5, controlado por TIM3 con intervalos de 250 ms.

---

## Qué es diferente respecto a bare-metal

En la programación bare-metal configurabas los periféricos escribiendo directamente
en los registros:

```c
RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
GPIOA->MODER |= (1 << 10);
```

Con HAL, las mismas operaciones ocurren dentro de funciones de la librería. El acceso
a los registros sigue estando ahí — HAL simplemente lo envuelve. Tu trabajo es llamar
a las funciones correctas, en el orden correcto y con los parámetros correctos.

La macro de habilitación de reloj que verás en breve:

```c
__HAL_RCC_GPIOA_CLK_ENABLE();
```

hace exactamente lo mismo que `RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN`. Misma operación
de hardware, distinta sintaxis.

---

## Ubicación del paquete de firmware

El paquete de firmware STM32CubeF4 ya está instalado en tu máquina. La ruta es:

```
~/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/
```

Dentro de esa carpeta encontrarás:

```
STM32Cube_FW_F4_V1.28.3/
├── Drivers/
│   ├── CMSIS/
│   │   └── Device/ST/STM32F4xx/Source/Templates/
│   │       └── system_stm32f4xx.c              ← necesitas este archivo
│   └── STM32F4xx_HAL_Driver/
│       ├── Inc/                                 ← todos los archivos de cabecera .h
│       └── Src/                                 ← todos los archivos fuente .c
```

Los archivos fuente de HAL (`.c`) se copiarán a tu proyecto. Los archivos de cabecera
(`.h`) se quedarán aquí y se referenciarán mediante rutas de inclusión en
`CMakeLists.txt` — no hace falta copiar las cabeceras. No modifiques los originales —
siempre copia, nunca muevas.

---

## Paso 1 — Crear un proyecto vacío nuevo en VS Code

Usa el mismo asistente que usaste para el proyecto `bare-metal-stm32`. En la barra
lateral de STM32Cube de VS Code, haz clic en **"Create empty project"** (o abre la
Paleta de Comandos con `Ctrl+Shift+P` y escribe "STM32"):

1. Dale un nombre al proyecto y elige una carpeta.
2. Placa: **NUCLEO-F411RE**.
3. Tipo de proyecto: **CMake**, toolchain: **GCC**.
4. Genera el proyecto y ábrelo ("Open in this window").

No uses STM32CubeMX — el objetivo de este tutorial es escribir cada pieza a mano.

Después de la creación, la extensión te entrega un proyecto casi vacío:

```
TuProyecto/
├── build/
│   └── Debug/                  ← salida de compilación, generada por CMake
├── cmake/
│   ├── components.cmake
│   ├── files.cmake             ← lista los 4 archivos fuente de abajo
│   ├── flags.cmake
│   ├── gcc-arm-none-eabi.cmake ← archivo de toolchain (compilador y flags de FPU)
│   └── target.cmake
├── CMakeLists.txt              ← editarás este archivo
├── CMakePresets.json
├── Core/
│   ├── Inc/                    ← aquí van tus cabeceras
│   └── Src/
│       ├── main.c              ← reemplazarás su contenido
│       ├── startup_stm32f411xx.S
│       ├── syscall.c
│       └── sysmem.c
├── project-description.json
└── stm32f411xe_flash.ld        ← linker script, no lo toques
```

Los archivos dentro de `cmake/` pertenecen a la extensión — puede regenerarlos en
cualquier momento. Mantén todas tus adiciones en `CMakeLists.txt`, exactamente como lo
hiciste en el proyecto `bare-metal-stm32`.

Este es tu punto de partida. La librería HAL todavía no es parte del proyecto —
la añadirás en los siguientes pasos.

---

## Paso 2 — Crear la estructura de carpetas del proyecto

Dentro de tu proyecto, crea una nueva estructura de carpetas en la raíz del proyecto:

```
TuProyecto/
├── Core/
│   ├── Inc/                         ← ya existe
│   └── Src/                         ← ya existe
├── Drivers/
│   └── STM32F4xx_HAL_Driver/
│       └── Src/                     ← esta la creas tú
```

Para crear carpetas en VS Code: en el panel Explorer, selecciona la raíz del proyecto
y haz clic en el ícono **New Folder**. Puedes escribir la ruta completa de una vez —
`Drivers/STM32F4xx_HAL_Driver/Src` — y VS Code crea los tres niveles.

Nota: no hay subcarpeta `Inc/` dentro de `Drivers/` — las cabeceras de HAL no se
copian al proyecto. Se referencian directamente desde el paquete de firmware
mediante rutas de inclusión (configuradas en el Paso 3).

---

## Paso 3 — Configurar las rutas de inclusión en CMakeLists.txt

Los archivos de cabecera de HAL **no se copian** al proyecto. En su lugar, le dices al
compilador dónde encontrarlos añadiendo rutas de inclusión en `CMakeLists.txt`. Esto
evita duplicar cientos de archivos en cada proyecto.

Abre `CMakeLists.txt`. Cerca del inicio, después de la línea `project(...)`, define
una variable que apunte al paquete de firmware. Si conservaste la variable
`CMSIS_ROOT` del proyecto `bare-metal-stm32`, es la misma línea:

```cmake
# Ruta al paquete de firmware STM32Cube (ajusta la versión si cambia)
set(CMSIS_ROOT "$ENV{HOME}/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3")
```

Luego, en la definición del target, añade cuatro rutas de inclusión y el define del MCU:

```cmake
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc                             # stm32f4xx_hal_conf.h
    ${CMSIS_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc           # cabeceras HAL   ← nuevo
    ${CMSIS_ROOT}/Drivers/CMSIS/Core/Include                 # core_cm4.h
    ${CMSIS_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include  # stm32f4xx.h
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    STM32F411xE
)
```

Nota: las dos rutas de CMSIS y el define `STM32F411xE` son los mismos que ya usaste
en las semanas de bare-metal. Solo la ruta `Inc/` de HAL es nueva.
(`Core/Inc` también la añade la extensión en `cmake/flags.cmake` — listarla otra vez
aquí es inofensivo y deja la lista completa.)

Las rutas de inclusión solo resuelven la mitad del problema: permiten que el compilador
encuentre las **cabeceras**. Los **archivos fuente** necesitan su propia lista — eso
viene en el Paso 9.

---

## Paso 4 — Copiar solo los archivos fuente de HAL necesarios

**No** copies todos los archivos fuente — copia únicamente los listados a continuación.
Copiarlos todos causa fallos de compilación por definiciones en conflicto.

Ubicación de origen:
```
~/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers/STM32F4xx_HAL_Driver/Src/
```

Copia estos 9 archivos en `TuProyecto/Drivers/STM32F4xx_HAL_Driver/Src/`:

```
stm32f4xx_hal.c             ← HAL_Init(), HAL_IncTick()
stm32f4xx_hal_rcc.c         ← HAL_RCC_OscConfig(), HAL_RCC_ClockConfig()
stm32f4xx_hal_gpio.c        ← HAL_GPIO_Init(), HAL_GPIO_TogglePin()
stm32f4xx_hal_tim.c         ← HAL_TIM_Base_Init(), HAL_TIM_Base_Start_IT()
stm32f4xx_hal_tim_ex.c      ← requerido internamente por stm32f4xx_hal_tim.c
stm32f4xx_hal_dma.c         ← acompañante del módulo DMA (ver la lista de verificación)
stm32f4xx_hal_cortex.c      ← HAL_NVIC_EnableIRQ()
stm32f4xx_hal_flash.c       ← acompañante del módulo FLASH (ver la lista de verificación)
stm32f4xx_hal_pwr.c         ← acompañante del módulo PWR (ver la lista de verificación)
```

Nota: `stm32f4xx_hal_tim_ex.c` no se usa directamente en tu código, pero el linker lo
requiere: `HAL_TIM_IRQHandler()` dentro de `stm32f4xx_hal_tim.c` llama a funciones
que están definidas en `stm32f4xx_hal_tim_ex.c`.

Copiar los archivos no es suficiente — CMake no compila un archivo solo porque esté
dentro de la carpeta del proyecto. Los registrarás en el Paso 9.

---

## La relación entre archivo fuente y archivo de configuración

Esta es una de las cosas más importantes que hay que entender al construir un proyecto
HAL sin CubeMX. Cada driver de periférico tiene **dos piezas** que siempre deben existir
juntas — y, en un proyecto CMake, una **tercera** que conecta el archivo fuente con la
compilación:

```
stm32f4xx_hal_xxx.c       ←→   #define HAL_XXX_MODULE_ENABLED   ←→   línea en target_sources()
  (archivo fuente en              (define en                          (en CMakeLists.txt,
   Drivers/Src/)                   stm32f4xx_hal_conf.h)               Paso 9)
```

- El archivo `.c` contiene el código real del driver — las funciones que llamas.
- El `#define` en `stm32f4xx_hal_conf.h` le indica al compilador que active ese
  driver. Sin él, los archivos de cabecera de HAL no exponen la API de ese
  periférico — ni tipos, ni funciones, ni macros.
- La línea en `target_sources()` le dice a CMake que compile el archivo `.c`. Un
  archivo `.c` que CMake no conoce es invisible: para el linker es exactamente lo
  mismo que un archivo ausente.

Si **falta uno**, el resultado es un fallo de compilación:

```
falta el #define                      →  error del compilador: unknown type name,
                                          undeclared identifier, implicit declaration
falta el .c, o no está en CMake       →  error del linker: undefined reference
```

Mantén las tres piezas juntas, siempre.

### Lista de verificación para este proyecto (GPIO + TIM3)

Usa esta tabla para verificar tu proyecto antes de compilar. Cada fila debe tener
todas las columnas marcadas.

| Define en `stm32f4xx_hal_conf.h` | Archivo fuente en `Drivers/Src/` (y en CMake) | Por qué está aquí |
|----------------------------------|-----------------------------------------------|-------------------|
| `HAL_MODULE_ENABLED`             | `stm32f4xx_hal.c`        | HAL núcleo: `HAL_Init()`, `HAL_IncTick()` |
| `HAL_RCC_MODULE_ENABLED`         | `stm32f4xx_hal_rcc.c`    | Configuración del árbol de reloj, macros de habilitación de reloj |
| `HAL_GPIO_MODULE_ENABLED`        | `stm32f4xx_hal_gpio.c`   | Inicialización y control de GPIO |
| `HAL_TIM_MODULE_ENABLED`         | `stm32f4xx_hal_tim.c`    | Driver base de temporizador |
| *(no necesita define)*           | `stm32f4xx_hal_tim_ex.c` | Llamado por `HAL_TIM_IRQHandler()` |
| `HAL_DMA_MODULE_ENABLED`         | `stm32f4xx_hal_dma.c`    | El handle de TIM contiene punteros `DMA_HandleTypeDef` |
| `HAL_CORTEX_MODULE_ENABLED`      | `stm32f4xx_hal_cortex.c` | NVIC y SysTick: `HAL_NVIC_EnableIRQ()`, usado por `HAL_Init()` |
| `HAL_FLASH_MODULE_ENABLED`       | `stm32f4xx_hal_flash.c`  | Macros de caché/prefetch en `HAL_Init()`, `FLASH_LATENCY_0` |
| `HAL_PWR_MODULE_ENABLED`         | `stm32f4xx_hal_pwr.c`    | Aún no se usa — necesario al configurar el PLL a 100 MHz |

Observa el caso especial de `stm32f4xx_hal_tim_ex.c` — es un archivo acompañante de
`stm32f4xx_hal_tim.c` y no tiene su propio `#define`. Debe estar presente en el
proyecto (y en CMake) pero no requiere ninguna entrada en `stm32f4xx_hal_conf.h`.

**¿Por qué incluir módulos que este blinky apenas usa?** Para DMA, FLASH y PWR, este
proyecto necesita las *cabeceras* (el `#define`), pero todavía no se llama a ninguna
de las funciones dentro de sus archivos `.c`. Se mantienen de todos modos, por dos
razones: el linker elimina toda función que nadie llama (el flag `--gc-sections` en
`cmake/gcc-arm-none-eabi.cmake`), así que no cuestan ni un byte en el binario; y el
día que llames a `HAL_TIM_Base_Start_DMA()` o configures el PLL, el driver ya estará
ahí. Mantener juntos el define y el archivo es el hábito que previene los errores
anteriores.

---

## Paso 5 — Copiar el archivo de sistema de CMSIS

Copia `system_stm32f4xx.c` desde las plantillas de CMSIS:

Origen:
```
~/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
```

Destino:
```
TuProyecto/Core/Src/system_stm32f4xx.c
```

> **¿Vienes de `bare-metal-stm32`?** Si tu `CMakeLists.txt` ya compila este archivo
> directamente desde el paquete de firmware (una línea que termina en
> `.../Source/Templates/system_stm32f4xx.c`), elimina esa línea. El archivo debe
> compilarse exactamente una vez — dos copias producen `multiple definition of 'SystemCoreClock'`.

No necesitas modificar este archivo — úsalo tal cual. Pero vale la pena entender lo
que realmente hace, porque se ejecuta antes de `main()` y sus efectos siempre están
activos.

### Qué hace realmente system_stm32f4xx.c

El archivo provee tres cosas al resto del sistema:

**`SystemInit()`** — llamada automáticamente por el archivo de arranque
(`startup_stm32f411xx.S`) inmediatamente después del reset, antes de llegar a
`main()`. Para el STM32F411 hace exactamente dos cosas:

```
1. Habilita la FPU
   SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));

   Esto concede acceso completo a los coprocesadores CP10 y CP11, que son la
   unidad de punto flotante del Cortex-M4. Sin esta línea, cualquier instrucción
   de punto flotante provocaría un hard fault. Solo se ejecuta cuando el toolchain
   está configurado para FPU por hardware (__FPU_PRESENT == 1 y __FPU_USED == 1).

2. Reubicación de la tabla de vectores (condicional, deshabilitada por defecto)
   SCB->VTOR = VECT_TAB_BASE_ADDRESS | VECT_TAB_OFFSET;

   Esto solo se ejecuta si USER_VECT_TAB_ADDRESS está definido — lo cual no
   ocurre por defecto. En un proyecto estándar, el valor de reset por hardware de
   SCB->VTOR ya apunta a Flash en 0x08000000, que es correcto. El archivo de
   arranque define la tabla de vectores allí; este archivo no la mueve a menos
   que lo pidas explícitamente.
```

**`SystemCoreClock`** — una variable global inicializada en 16000000 (frecuencia del HSI).
Esta variable se actualiza automáticamente cada vez que llamas a
`HAL_RCC_ClockConfig()` en tu `SystemClock_Config()`. El resto de HAL la usa para
calcular timeouts, baud rates y otros valores que dependen de la frecuencia. Siempre
debe reflejar la frecuencia real actual de HCLK.

**`AHBPrescTable[]` y `APBPrescTable[]`** — tablas de consulta usadas internamente por
`SystemCoreClockUpdate()` para calcular las frecuencias de los buses a partir de los
campos divisores de los registros RCC. No las llamas directamente, pero HAL las usa.

### Qué NO hace system_stm32f4xx.c

**No** configura el reloj del sistema. A diferencia de lo que CubeMX solía generar,
este archivo deja el árbol de reloj exactamente como lo dejó el reset de hardware —
HSI encendido, sin PLL, todos los divisores en 1. La configuración del reloj es
enteramente responsabilidad tuya en `SystemClock_Config()` dentro de `main.c`.

### Cómo verificar la configuración de la FPU

Puedes verificar que ambos flags estén correctamente definidos añadiendo este bloque
en `main.c`, **después** de la línea `#include "stm32f4xx_hal.h"` (antes de ella, las
dos macros todavía no existen y la prueba siempre dice "NOT enabled"):

```c
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    #pragma message("FPU: enabled")
#else
    #pragma message("FPU: NOT enabled")
#endif
```

Esto produce un mensaje en la salida de compilación durante la compilación — sin
costo en tiempo de ejecución, sin añadir nada al binario. El mensaje solo aparece
cuando `main.c` se recompila realmente (después de un cambio, o tras una compilación
limpia).

**Importante:** el editor de VS Code atenuará en gris la rama inactiva, haciendo que
parezca que esas líneas se ignoran. No confíes en el color gris — es solo el análisis
del editor. El compilador es un proceso completamente separado. La verdad siempre
está en la salida de compilación, donde verás:

```
note: '#pragma message: FPU: enabled'
```

También puedes confirmar el estado de la FPU directamente desde la línea de comandos
del compilador, sin necesidad del `#pragma`. Ejecuta una compilación verbose en la
terminal (`cmake --build --preset Debug --verbose`) o abre
`build/Debug/compile_commands.json`, y busca estos dos flags en la invocación de
`arm-none-eabi-gcc`:

```
-mfpu=fpv4-sp-d16      ← especifica la unidad de hardware FPU
-mfloat-abi=hard       ← establece __FPU_USED = 1, habilita instrucciones float por hardware
```

Si en cambio ves `-mfloat-abi=soft` (o ningún `-mfpu`), `__FPU_USED` es 0,
`SystemInit()` omitirá la activación de la FPU, y cualquier operación de punto
flotante provocará un hard fault en tiempo de ejecución. En este proyecto esos flags
vienen de los archivos de la extensión:

```
cmake/target.cmake          set(CMSIS_Dfpu SP_FPU)       ← la elección de placa del Paso 1
cmake/gcc-arm-none-eabi.cmake                            ← convierte SP_FPU en los dos flags
```

---

## Paso 6 — Crear el archivo de configuración de HAL

El archivo de configuración de HAL le indica al compilador qué módulos de periférico
están activos. Debes crear este archivo manualmente.

Copia la plantilla desde el paquete de firmware:

Origen:
```
~/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_conf_template.h
```

Destino:
```
TuProyecto/Core/Inc/stm32f4xx_hal_conf.h
```

Nota: renómbralo — quita `_template` del nombre del archivo.

Ahora abre `stm32f4xx_hal_conf.h`. Encuentra la sección titulada **Module Selection** —
habilita todos los módulos de la familia, uno por línea. Reemplaza toda esa lista
únicamente con los módulos que necesitas para este proyecto:

```c
/* Módulos activos para el proyecto GPIO + TIM3 */
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
```

Comenta o elimina todas las demás líneas `HAL_xxx_MODULE_ENABLED`. Deja el resto del
archivo como está.

Nota: unas líneas más abajo verás `#define HSE_VALUE 25000000U`. Hoy no importa (usamos
HSI), pero recuérdalo: la NUCLEO-F411RE no tiene cristal para HSE — recibe un reloj de
8 MHz desde el ST-LINK. Cuando pases a HSE en una sesión futura, este valor debe
cambiarse a `8000000U`.

---

## Paso 7 — Crear el archivo de manejadores de interrupción

Crea `TuProyecto/Core/Src/stm32f4xx_it.c` con el siguiente contenido:

```c
/*
 * stm32f4xx_it.c
 * Rutinas de servicio de interrupción
 * Autor: tu nombre
 */

#include "stm32f4xx_hal.h"

/* Declara el handle de TIM3 — definido en main.c */
extern TIM_HandleTypeDef htim3;

/* Manejador de SysTick — requerido por HAL para HAL_Delay() y los timeouts */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* Manejador del evento de actualización de TIM3 */
void TIM3_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim3);
}
```

Los nombres `SysTick_Handler` y `TIM3_IRQHandler` no son arbitrarios — deben coincidir
exactamente con los nombres de la tabla de vectores de `startup_stm32f411xx.S`.

---

## Paso 8 — Escribir main.c

Reemplaza el contenido de `Core/Src/main.c` con lo siguiente:

```c
/*
 * main.c
 * HAL Blinky — LED en PA5 conmutado cada 250 ms por TIM3
 * Autor: tu nombre
 */

#include "stm32f4xx_hal.h"

/* Handle de TIM3 — debe ser global para que stm32f4xx_it.c pueda accederlo */
TIM_HandleTypeDef htim3;

/* Prototipos de funciones privadas */
static void SystemClock_Config(void);
static void gpio_Init(void);
static void tim3_Init(void);

int main(void)
{
    HAL_Init();           /* inicializa HAL: SysTick, caché, agrupación de prioridades */
    SystemClock_Config(); /* configura el árbol de reloj: HSI a 16 MHz                 */
    gpio_Init();          /* configura PA5 como salida push-pull                       */
    tim3_Init();          /* configura TIM3: evento de actualización cada 250 ms       */

    while (1)
    {
        /* lazo de aplicación — la conmutación del LED ocurre en el callback */
    }
}

/*
 * SystemClock_Config
 * Usa el oscilador interno HSI a 16 MHz
 * Sin PLL — la configuración de reloj más simple posible
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* HSI ya está encendido tras el reset — se confirma y se usa */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* Selecciona HSI como SYSCLK — todos los divisores de bus en 1 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_HCLK   |
                                       RCC_CLOCKTYPE_PCLK1  |
                                       RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 16 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;     /* APB1  = 16 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2  = 16 MHz */

    /* FLASH_LATENCY_0 = cero estados de espera, correcto para 16 MHz */
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

/*
 * gpio_Init
 * Configura PA5 como salida push-pull — LED integrado en la placa Nucleo
 */
static void gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Habilita el reloj de GPIOA en el bus AHB1
       Igual que en bare-metal: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Configura PA5 */
    GPIO_InitStruct.Pin   = GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/*
 * tim3_Init
 * Configura TIM3 para generar un evento de actualización cada 250 ms
 *
 * Cadena de reloj:
 *   HSI (16 MHz) → APB1 (16 MHz) → reloj de TIM3 (16 MHz)
 *
 * PSC = 15999  →  tick = 16,000,000 / (15999 + 1) = 1,000 Hz  (1 ms por tick)
 * ARR = 249    →  periodo = (249 + 1) x 1 ms = 250 ms
 */
static void tim3_Init(void)
{
    /* Habilita el reloj de TIM3 en el bus APB1 */
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* Configura la base de TIM3 */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 15999;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 249;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(&htim3);

    /* Inicia TIM3 en modo interrupción — habilita la interrupción del evento de actualización */
    HAL_TIM_Base_Start_IT(&htim3);

    /* Habilita la línea de interrupción de TIM3 en el NVIC */
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

/*
 * HAL_TIM_PeriodElapsedCallback
 * Llamada automáticamente por HAL_TIM_IRQHandler() cada vez que se dispara
 * un evento de actualización de un temporizador. Compartida por todos los
 * temporizadores — siempre verifica htim->Instance.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }
}
```

`main.c` ya es parte de la compilación — la extensión lo listó en
`cmake/files.cmake` cuando creó el proyecto. Los archivos que añadiste en los Pasos 4,
5 y 7 no. Ese es el siguiente paso.

---

## Paso 9 — Indicarle a CMake qué archivos compilar

CMake solo compila los archivos que se le indican. Ahora mismo conoce los cuatro
archivos que creó el asistente (`main.c`, `startup_stm32f411xx.S`, `syscall.c`,
`sysmem.c`, listados en `cmake/files.cmake`). Todo lo que copiaste o creaste desde
entonces sigue siendo invisible para la compilación.

Abre `CMakeLists.txt` y añade un bloque `target_sources()` después de
`add_executable(...)`, junto a las rutas de inclusión del Paso 3:

```cmake
# Añade fuentes al ejecutable
target_sources(${PROJECT_NAME} PRIVATE
    # Archivos del proyecto (main.c, startup, syscall.c y sysmem.c ya están
    # listados por la extensión en cmake/files.cmake — no los repitas)
    ${CMAKE_SOURCE_DIR}/Core/Src/stm32f4xx_it.c
    ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f4xx.c

    # Drivers HAL — una línea por archivo copiado en el Paso 4
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
)
```

No añadas estas líneas a `cmake/files.cmake` — ese archivo pertenece a la
extensión y puede ser sobrescrito.

La lista se escribe a mano a propósito. CMake podría buscar en las carpetas
automáticamente, pero una lista explícita es la versión CMake de la lista de
verificación: ves exactamente qué drivers están en la compilación, y nada entra por
accidente.

Al guardar `CMakeLists.txt`, VS Code reconfigura el proyecto automáticamente.
Si no lo hace, ejecuta `Ctrl+Shift+P` → **CMake: Configure**.

> **Cuidado con `stm32f4xx_it.c`.** Si olvidas esta línea, el proyecto igual compila
> sin un solo error ni advertencia — el archivo de arranque provee un manejador por
> defecto para cada interrupción. Pero ese manejador por defecto es un lazo infinito:
> la primera interrupción de SysTick (1 ms después de `HAL_Init()`) atrapa la CPU ahí
> para siempre, y el LED nunca parpadea.

---

## Paso 10 — Estructura final del proyecto

Después de todos los pasos, tu proyecto debe verse exactamente así:

```
TuProyecto/
│
├── cmake/                           ← archivos de la extensión, no editar
├── CMakeLists.txt                   ← rutas de inclusión (Paso 3) + fuentes (Paso 9)
│
├── Core/
│   ├── Inc/
│   │   └── stm32f4xx_hal_conf.h
│   └── Src/
│       ├── main.c
│       ├── startup_stm32f411xx.S    ← creado por el asistente
│       ├── stm32f4xx_it.c
│       ├── syscall.c                ← creado por el asistente
│       ├── sysmem.c                 ← creado por el asistente
│       └── system_stm32f4xx.c
│
├── Drivers/
│   └── STM32F4xx_HAL_Driver/
│       └── Src/
│           ├── stm32f4xx_hal.c
│           ├── stm32f4xx_hal_rcc.c
│           ├── stm32f4xx_hal_gpio.c
│           ├── stm32f4xx_hal_tim.c
│           ├── stm32f4xx_hal_tim_ex.c
│           ├── stm32f4xx_hal_dma.c
│           ├── stm32f4xx_hal_cortex.c
│           ├── stm32f4xx_hal_flash.c
│           └── stm32f4xx_hal_pwr.c
│
└── stm32f411xe_flash.ld
```

Rutas de inclusión configuradas en `CMakeLists.txt` (no visibles en el árbol del proyecto):
```
${CMAKE_SOURCE_DIR}/Core/Inc
${CMSIS_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc
${CMSIS_ROOT}/Drivers/CMSIS/Core/Include
${CMSIS_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
```

---

## Paso 11 — Compilar y flashear

**Compilar.** Haz clic en **Build** en la barra de estado de VS Code, presiona `F7`, o
ejecuta `Ctrl+Shift+P` → **CMake: Build**. Una compilación exitosa termina con el
reporte de memoria:

```
[16/16] Linking C executable YourProject.elf
Memory region         Used Size  Region Size  %age Used
             RAM:        2064 B       128 KB      1.57%
           FLASH:        7304 B       512 KB      1.39%
```

Tus números pueden diferir ligeramente. 16 pasos = 9 archivos HAL + 6 archivos del
proyecto + enlazado.

**Flashear y ejecutar.** Abre la vista **Run and Debug** (`Ctrl+Shift+D`), selecciona
la configuración **STM32Cube: Launch ST-Link GDB Server** y presiona `F5`. VS Code
compila el proyecto, flashea la placa y se detiene en la primera línea de `main()`.
Presiona `F5` (Continue) otra vez para dejar correr el programa.

El LED integrado en PA5 debería parpadear a 2 Hz (250 ms encendido, 250 ms apagado).

---

## Errores de compilación comunes y sus soluciones

Estos errores se reprodujeron mientras se construía este tutorial. Si ves uno, sabes
exactamente qué añadir. Están listados en el orden de los pasos.

```
fatal error: stm32f4xx_hal.h: No such file or directory
```
Solución: falta la ruta de inclusión `Inc/` de HAL en `CMakeLists.txt` (Paso 3).

```
error: unknown type name 'DMA_HandleTypeDef'
```
Solución: añade `#define HAL_DMA_MODULE_ENABLED` a `stm32f4xx_hal_conf.h`.

```
error: 'FLASH_LATENCY_0' undeclared
error: implicit declaration of function '__HAL_FLASH_INSTRUCTION_CACHE_ENABLE'
```
Solución: añade `#define HAL_FLASH_MODULE_ENABLED` a `stm32f4xx_hal_conf.h`.

```
error: implicit declaration of function 'HAL_NVIC_EnableIRQ'
```
Solución: añade `#define HAL_CORTEX_MODULE_ENABLED` a `stm32f4xx_hal_conf.h`.

```
undefined reference to 'HAL_Init'
undefined reference to 'HAL_GPIO_Init'
undefined reference to 'HAL_TIM_Base_Init'   (y así sucesivamente)
```
Solución: los archivos `.c` de HAL están copiados pero no listados en
`target_sources()` (Paso 9) — o no se copiaron en absoluto (Paso 4). Cada nombre en
el mensaje señala el archivo que falta.

```
undefined reference to 'HAL_TIMEx_BreakCallback'
undefined reference to 'HAL_TIMEx_CommutCallback'
```
Solución: copia `stm32f4xx_hal_tim_ex.c` a `Drivers/STM32F4xx_HAL_Driver/Src/` **y**
añádelo a `target_sources()`.

```
undefined reference to 'SystemCoreClock'
undefined reference to 'AHBPrescTable'
```
Solución: `system_stm32f4xx.c` no está en la compilación (Pasos 5 y 9).

```
multiple definition of 'SystemCoreClock'
```
Solución: `system_stm32f4xx.c` se compila dos veces — normalmente la copia en
`Core/Src/` más una línea antigua del proyecto bare-metal que apunta al paquete de
firmware. Deja solo una (Paso 5).

**Sin error, pero el LED no parpadea.** Dos causas silenciosas, ambas relacionadas con
nombres que deben escribirse exactamente bien:
- `stm32f4xx_it.c` no está en `target_sources()` (Paso 9). Pausa el depurador: si se
  detiene dentro de `Default_Handler`, este es tu problema.
- `HAL_TIM_PeriodElapsedCallback` está mal escrita. HAL contiene una versión vacía por
  defecto de esta función; si tu nombre no coincide, HAL usa silenciosamente la vacía
  y el LED nunca conmuta.

---

## Resumen de conceptos clave

```
CMakeLists.txt
    │
    ├── target_include_directories()   → el compilador encuentra los archivos .h
    └── target_sources()               → los archivos .c se compilan y enlazan

HAL_Init()
    │
    ├── habilita la caché de instrucciones de Flash, la caché de datos y el prefetch
    ├── establece la agrupación de prioridades del NVIC
    ├── inicializa SysTick a 1 ms
    └── llama a HAL_MspInit() (vacía por defecto)

SystemClock_Config()
    │
    └── HSI ON → SYSCLK = 16 MHz → AHB = 16 MHz → APB1 = 16 MHz

gpio_Init()
    │
    ├── __HAL_RCC_GPIOA_CLK_ENABLE()   (igual que RCC->AHB1ENR |= ...)
    └── HAL_GPIO_Init(GPIOA, &struct)

tim3_Init()
    │
    ├── __HAL_RCC_TIM3_CLK_ENABLE()
    ├── HAL_TIM_Base_Init(&htim3)
    ├── HAL_TIM_Base_Start_IT(&htim3)
    └── HAL_NVIC_EnableIRQ(TIM3_IRQn)

TIM3_IRQHandler()   [en stm32f4xx_it.c]
    │
    └── HAL_TIM_IRQHandler(&htim3)
            │
            └── HAL_TIM_PeriodElapsedCallback(&htim3)
                    │
                    └── HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5)
```

Referencia: UM1725 Rev 8 — Description of STM32F4 HAL and low-layer drivers
(STMicroelectronics, marzo de 2023)

---

---

# Fase 2 — Añadir un módulo nuevo: USART2

En este punto el proyecto funciona — LED parpadeando, interrupción de TIM3 disparándose
cada 250 ms. La Fase 2 muestra cómo añadir un nuevo periférico a un proyecto existente,
siguiendo exactamente el mismo patrón usado para TIM3. El proceso siempre es el mismo:

```
1. Añadir el archivo fuente a Drivers/Src/
2. Añadir el archivo fuente a target_sources() en CMakeLists.txt
3. Añadir el #define a stm32f4xx_hal_conf.h
4. Añadir el handle al inicio de main.c
5. Escribir la función de inicialización
6. Llamarla desde main()
```

## Paso 1 — Añadir el archivo fuente de USART2

Copia un archivo nuevo en `Drivers/STM32F4xx_HAL_Driver/Src/`:

```
stm32f4xx_hal_uart.c        ← HAL_UART_Init(), HAL_UART_Transmit()
```

## Paso 2 — Añadirlo a CMake

Abre `CMakeLists.txt` y añade una línea al final de la lista de drivers HAL en
`target_sources()`:

```cmake
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
```

## Paso 3 — Añadir el define del módulo

Abre `stm32f4xx_hal_conf.h` y añade una línea:

```c
#define HAL_UART_MODULE_ENABLED
```

Lista de verificación actualizada — ahora cubre GPIO + TIM3 + USART2:

| Define en `stm32f4xx_hal_conf.h` | Archivo fuente en `Drivers/Src/` (y en CMake) | Por qué está aquí |
|----------------------------------|-----------------------------------------------|-------------------|
| `HAL_MODULE_ENABLED`             | `stm32f4xx_hal.c`        | HAL núcleo: `HAL_Init()`, `HAL_IncTick()` |
| `HAL_RCC_MODULE_ENABLED`         | `stm32f4xx_hal_rcc.c`    | Configuración del árbol de reloj, macros de habilitación de reloj |
| `HAL_GPIO_MODULE_ENABLED`        | `stm32f4xx_hal_gpio.c`   | Inicialización y control de GPIO |
| `HAL_TIM_MODULE_ENABLED`         | `stm32f4xx_hal_tim.c`    | Driver base de temporizador |
| *(no necesita define)*           | `stm32f4xx_hal_tim_ex.c` | Llamado por `HAL_TIM_IRQHandler()` |
| `HAL_DMA_MODULE_ENABLED`         | `stm32f4xx_hal_dma.c`    | Los handles de TIM y UART contienen punteros `DMA_HandleTypeDef` |
| `HAL_CORTEX_MODULE_ENABLED`      | `stm32f4xx_hal_cortex.c` | NVIC y SysTick: `HAL_NVIC_EnableIRQ()`, usado por `HAL_Init()` |
| `HAL_FLASH_MODULE_ENABLED`       | `stm32f4xx_hal_flash.c`  | Macros de caché/prefetch en `HAL_Init()`, `FLASH_LATENCY_0` |
| `HAL_PWR_MODULE_ENABLED`         | `stm32f4xx_hal_pwr.c`    | Aún no se usa — necesario al configurar el PLL a 100 MHz |
| `HAL_UART_MODULE_ENABLED`        | `stm32f4xx_hal_uart.c`   | Transmisión y recepción UART/USART |

Si olvidas el define, el error es de la misma familia que antes:
`unknown type name 'UART_HandleTypeDef'`.

## Paso 4 — Añadir el handle de USART2

Igual que `htim3`, el handle debe ser global. Añádelo junto a `htim3` al inicio de
`main.c`, junto con el prototipo de la nueva función de inicialización:

```c
TIM_HandleTypeDef  htim3;
UART_HandleTypeDef huart2;   /* ← añade esta línea */

static void usart2_Init(void);   /* ← y esta, junto con los demás prototipos */
```

## Paso 5 — Escribir la función usart2_Init()

Compárala con `tim3_Init()` — la estructura es idéntica. El único elemento nuevo es el
bloque de función alterna de GPIO al inicio. USART2 usa pines físicos (PA2 para TX,
PA3 para RX), así que esos pines deben configurarse como función alterna antes de
inicializar el periférico.

Los pines GPIO de salida como el LED en PA5 no necesitan función alterna — la CPU los
maneja directamente. Los periféricos de comunicación sí, porque la señal la genera el
hardware del periférico, no la CPU. Todo periférico que use pines — USART, SPI, I2C,
I2S — requerirá este paso.

```c
static void usart2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Paso 1: habilita los relojes — tanto USART2 como GPIOA */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();   /* ya se llama en gpio_Init() — es seguro llamarla de nuevo */

    /* Paso 2: configura PA2 (TX) y PA3 (RX) como función alterna
       AF7 es USART2 en el STM32F411 — ver la tabla de funciones alternas en el datasheet */
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Paso 3: configura USART2 */
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}
```

## Paso 6 — Llamar a usart2_Init() desde main()

Añade la llamada después de `tim3_Init()`:

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    gpio_Init();
    tim3_Init();
    usart2_Init();       /* ← añade esta línea */

    while (1)
    {
    }
}
```

## Paso 7 — Transmitir datos

Con el periférico inicializado, enviar datos es una sola llamada:

```c
/* Envía una cadena — modo bloqueante, timeout de 100 ms */
char msg[] = "Hello from STM32\r\n";
HAL_UART_Transmit(&huart2, (uint8_t *)msg, sizeof(msg) - 1, 100);
```

Para enviar un mensaje cada 250 ms, usa el mismo patrón del resto del curso:
**la interrupción levanta una bandera, el lazo principal hace el trabajo.**

```c
volatile uint8_t send_msg_flag = 0;     /* global, junto a los handles */

int main(void)
{
    /* ... llamadas de inicialización ... */

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

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        send_msg_flag = 1;                /* ← solo levanta la bandera */
    }
}
```

**¿Por qué no llamar a `HAL_UART_Transmit()` directamente dentro del callback?** El
callback se ejecuta dentro de la interrupción de TIM3. TIM3 tiene prioridad 0 (la más
alta — nunca la cambiamos), mientras que SysTick tiene prioridad 15 (la más baja,
`TICK_INT_PRIORITY` en `stm32f4xx_hal_conf.h`). Mientras la interrupción de TIM3 se
está ejecutando, SysTick no puede interrumpirla, así que `HAL_GetTick()` deja de
contar: el timeout de 100 ms de `HAL_UART_Transmit()` nunca puede expirar, y un
`HAL_Delay()` dentro de un callback espera para siempre. Mantén las interrupciones
cortas; deja las llamadas bloqueantes para el lazo principal.

Para ver los mensajes, abre una terminal serial en el puerto COM virtual del ST-LINK
(`/dev/ttyACM0` en Linux) a 115200 baudios, 8N1.

## Qué es diferente en comparación con TIM3

```
tim3_Init()                        usart2_Init()
─────────────────────────────────  ─────────────────────────────────
__HAL_RCC_TIM3_CLK_ENABLE()        __HAL_RCC_USART2_CLK_ENABLE()
                                   __HAL_RCC_GPIOA_CLK_ENABLE()
                                   Configurar PA2/PA3 como AF7   ← nuevo
Llenar TIM_HandleTypeDef           Llenar UART_HandleTypeDef
HAL_TIM_Base_Init(&htim3)          HAL_UART_Init(&huart2)
HAL_TIM_Base_Start_IT(&htim3)      (no se necesita Start para TX por polling)
HAL_NVIC_EnableIRQ(TIM3_IRQn)      (no se necesita NVIC para TX por polling)
```

---

---

# Apéndice — Caché de Flash y Prefetch

Cuando llamas a `HAL_Init()`, se habilitan internamente tres funciones de aceleración
de Flash: la **caché de instrucciones**, la **caché de datos** y el **buffer de
prefetch**. Aparecen en el código fuente de HAL y en comentarios a lo largo del código,
así que vale la pena entender qué hacen realmente.

## El problema que resuelven

El núcleo Cortex-M4 del STM32F411 puede funcionar hasta 100 MHz. La memoria Flash
interna no puede seguir ese ritmo — tiene un tiempo de acceso fijo que se convierte en
un cuello de botella a frecuencias de reloj más altas. Sin ninguna aceleración, la CPU
tendría que insertar estados de espera (ciclos inactivos) en cada lectura de Flash,
desperdiciando tiempo de ejecución.

A 16 MHz (HSI, sin PLL) esto no es un problema — Flash es lo bastante rápida y no se
necesitan estados de espera. Pero HAL habilita las funciones de todos modos, porque el
mismo código debe funcionar a cualquier frecuencia.

## Caché de instrucciones

```
Sin caché:                         Con caché:

La CPU solicita instrucción        La CPU solicita instrucción
       ↓                                  ↓
Lectura de Flash (lenta)           ¿Acierto de caché? → instantáneo
       ↓                                  ↓ (fallo)
La CPU ejecuta                     Lectura de Flash → se guarda en caché
                                          ↓
                                   La CPU ejecuta
                                   La próxima vez: acierto de caché → instantáneo
```

La caché de instrucciones es una memoria pequeña y rápida (1 KB: 64 líneas de 128
bits cada una) que almacena instrucciones recientemente leídas. Cuando la CPU ejecuta
un lazo, el cuerpo del lazo se lee de Flash una vez y se guarda en la caché. Cada
iteración posterior se sirve desde la caché a la velocidad completa de la CPU, sin
ningún acceso a Flash. Para el firmware embebido, que pasa la mayor parte del tiempo
en lazos y manejadores de interrupción, esto es una ganancia significativa.

La caché de datos funciona de la misma manera, pero es mucho más pequeña (8 líneas de
128 bits) y almacena datos constantes leídos de Flash — tablas de consulta, literales
de cadena.

## Buffer de prefetch

El buffer de prefetch funciona de forma distinta — en lugar de esperar a que la CPU
solicite la siguiente instrucción, lee por adelantado en Flash mientras la CPU todavía
está ejecutando la instrucción actual. Como Flash se accede en bloques de 128 bits y
las instrucciones del Cortex-M4 son de 16 o 32 bits, una sola lectura de Flash entrega
varias instrucciones a la vez. El buffer de prefetch mantiene listo el siguiente
bloque antes de que la CPU lo necesite.

```
La CPU ejecuta la instrucción N        Flash ya está leyendo la instrucción N+4, N+5...
La CPU ejecuta la instrucción N+1      (ya está en el buffer — sin espera)
La CPU ejecuta la instrucción N+2      (ya está en el buffer — sin espera)
```

## En la práctica para este curso

A 16 MHz con `FLASH_LATENCY_0` (cero estados de espera), estas funciones tienen un
efecto visible mínimo — Flash ya es lo bastante rápida. Su importancia crece cuando el
reloj se incrementa a 100 MHz mediante el PLL (sesiones futuras), donde se requiere
`FLASH_LATENCY_3` (tres estados de espera), y las cachés y el buffer de prefetch se
vuelven esenciales para mantener un rendimiento aceptable.

`HAL_Init()` habilita las tres según tres ajustes en `stm32f4xx_hal_conf.h` —
`INSTRUCTION_CACHE_ENABLE`, `DATA_CACHE_ENABLE` y `PREFETCH_ENABLE`, todos en `1U`
en la plantilla — de modo que el proyecto queda listo para cualquier configuración de
reloj sin requerir cambios en la secuencia de inicialización.
