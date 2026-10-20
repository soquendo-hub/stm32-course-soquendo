# Proyecto bare metal STM32

Proyecto de referencia basado en `resources/guia-proyecto-bare-metal-stm32-ES.md`.
Configura la NUCLEO-F411RE para alternar LD2 (PA5) mediante el evento de
actualización de TIM2 y su interrupción, sin HAL.

## Abrir y compilar

Abre esta carpeta como proyecto en VS Code, elige el preset `Debug` y ejecuta
`CMake: Build`. Los headers CMSIS se enlazan desde la instalación local de
STM32Cube indicada por `CMSIS_ROOT` en `CMakeLists.txt`.

El nombre de carpeta usa guiones bajos para seguir la convención de nombres del
curso. El proyecto requiere el paquete STM32Cube Firmware F4 indicado en
`CMSIS_ROOT`.
