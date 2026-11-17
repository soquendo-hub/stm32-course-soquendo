# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.20)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# Set target definition variables required by the toolchain file
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)

# Set the toolchain file for cross-compilation
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

# Project definition
project(Prueba_USART_HAL)

# Enable CMake support for ASM and C/CPP languages
enable_language(C CXX ASM)

set(PROJECT_NAME Prueba_USART_HAL)

set(CMSIS_ROOT "$ENV{HOME}/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0")
set(STM32_HAL_DRIVER_DIR "${CMSIS_ROOT}/Drivers/STM32F4xx_HAL_Driver")

# Create an executable object type
add_executable(${PROJECT_NAME})

# Include additional CMake files for flags, components, and files
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/flags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/components.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake)

# Añade fuentes al ejecutable
target_sources(${PROJECT_NAME} PRIVATE
    # Archivos del proyecto (main.c, startup, syscall.c y sysmem.c ya están
    # listados por la extensión en cmake/files.cmake — no los repitas)
    ${CMAKE_SOURCE_DIR}/Core/Src/stm32f4xx_it.c
    ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f4xx.c

    # HAL drivers, headers y fuentes tomados de la misma versión de STM32Cube.
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal.c
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal_rcc.c
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal_gpio.c
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal_tim.c
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal_tim_ex.c
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal_dma.c
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal_cortex.c
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal_flash.c
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal_pwr.c
    ${STM32_HAL_DRIVER_DIR}/Src/stm32f4xx_hal_uart.c

)


# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
  # User defined include directories
  ${CMAKE_SOURCE_DIR}/Core/Inc
  ${STM32_HAL_DRIVER_DIR}/Inc
  ${CMSIS_ROOT}/Drivers/CMSIS/Core/Include
  ${CMSIS_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
  # User defined compiler options
)

# Add additional defined symbols
target_compile_definitions(${PROJECT_NAME} PRIVATE
  # Add additional defined symbols here
  STM32F411xE
  USE_HAL_DRIVER
)

# Linker options
target_link_options(${PROJECT_NAME} PRIVATE
  # User defined linker options
)

# Add additional linker search directories
target_link_directories(${PROJECT_NAME} PRIVATE
  # Add additional linker search directories here
)

# Add additional linker libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
  # Add additional linker libraries here
)
