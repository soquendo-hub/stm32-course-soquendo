cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

project(bare_metal_stm32 C ASM)
enable_language(C ASM)

set(PROJECT_NAME bare_metal_stm32)
set(CMSIS_ROOT "/home/soquendo/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0")

add_executable(${PROJECT_NAME})

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake)

target_sources(${PROJECT_NAME} PRIVATE
	${CMSIS_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
	${CMSIS_ROOT}/Drivers/CMSIS/Core/Include
	${CMSIS_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE STM32F411xE)
target_compile_options(${PROJECT_NAME} PRIVATE
	${CPU_FLAGS}
	-fdata-sections
	-ffunction-sections
	-Wall
	-Wextra
	-Wpedantic
	-O0
	-g3
	-ggdb
)

target_link_options(${PROJECT_NAME} PRIVATE
	${CPU_FLAGS}
	-T${CMAKE_CURRENT_BINARY_DIR}/stm32f411xe_flash.ld
	-Wl,--gc-sections
	--specs=nosys.specs
	-Wl,--print-memory-usage
)

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")
