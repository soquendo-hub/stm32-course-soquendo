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
project(Tarea3_Etapa2)

# Enable CMake support for ASM and C/CPP languages
enable_language(C CXX ASM)

set(PROJECT_NAME Tarea3_Etapa2)

set(CMSIS_ROOT "$ENV{HOME}/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0")

# Create an executable object type
add_executable(${PROJECT_NAME})

# ---- Selector de aplicación -------------------------------------------
# Un main.c fijo, varias aplicaciones: APP decide qué app.c se compila.
# Cada etapa nueva añade su nombre a la lista STRINGS y una línea APP_DIR_<nombre>.
set(APP "blinky" CACHE STRING "Application to build")
set_property(CACHE APP PROPERTY STRINGS blinky polling interrupt)

set(APP_DIR_blinky    ${CMAKE_SOURCE_DIR}/Core/app_0_blinky)
set(APP_DIR_polling   ${CMAKE_SOURCE_DIR}/Core/app_1_polling)
set(APP_DIR_interrupt ${CMAKE_SOURCE_DIR}/Core/app_2_interrupt)





set(APP_DIR_blinky ${CMAKE_SOURCE_DIR}/Core/app_0_blinky)

if(NOT DEFINED APP_DIR_${APP})
    message(FATAL_ERROR "Unknown APP='${APP}': there is no APP_DIR_${APP} line in CMakeLists.txt")
endif()

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Core/common)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${APP_DIR_${APP}}/app.c)
message(STATUS ">>> Building APP = ${APP}")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/common/system.c
    ${CMAKE_SOURCE_DIR}/Core/common/heartbeat.c
    ${CMAKE_SOURCE_DIR}/Core/common/cmd_parser.c
    ${CMAKE_SOURCE_DIR}/Core/common/commands.c    
)



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
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c

)
  

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
  # User defined include directories
  ${CMAKE_SOURCE_DIR}/Core/Inc
  ${CMSIS_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc
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
