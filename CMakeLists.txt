
cmake_minimum_required(VERSION 3.28)
project(iot-shark-modern LANGUAGES C ASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SHARK_HOST_TEST "Build the host-side diagnostic tests" OFF)

add_library(shark_diagnostics STATIC diagnostics/can_diag.c diagnostics/sdram_diag.c)
target_include_directories(shark_diagnostics PUBLIC diagnostics)
target_compile_options(shark_diagnostics PRIVATE -Wall -Wextra -Werror)

if(SHARK_HOST_TEST)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Select a native C compiler for host-test")
    endif()
    enable_testing()
    add_executable(test_sdram tests/test_sdram.c)
    add_executable(test_can tests/test_can.c)
    foreach(test IN ITEMS test_sdram test_can)
        target_link_libraries(${test} PRIVATE shark_diagnostics)
        target_compile_options(${test} PRIVATE -Wall -Wextra -Werror)
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
    return()
endif()

if(NOT CMAKE_CROSSCOMPILING OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Configure the firmware with board-debug or board-release")
endif()

execute_process(COMMAND "${CMAKE_C_COMPILER}" --version
    OUTPUT_VARIABLE arm_gcc_version COMMAND_ERROR_IS_FATAL ANY)
if(NOT arm_gcc_version MATCHES "14[.]3[.]rel1|14[.]3[.]Rel1")
    message(FATAL_ERROR "Arm GNU Toolchain 14.3.Rel1 is required. Set ARM_GNU_TOOLCHAIN_ROOT.")
endif()

add_library(board_config INTERFACE)
target_include_directories(board_config INTERFACE board)
target_include_directories(board_config SYSTEM INTERFACE
    vendor/hal/Inc vendor/cmsis/Core vendor/cmsis/Device
    vendor/rt-thread/include vendor/rt-thread/finsh)
target_compile_definitions(board_config INTERFACE STM32H743xx USE_HAL_DRIVER __RTTHREAD__)
target_compile_options(board_config INTERFACE -ffunction-sections -fdata-sections)

# Object targets keep the RT-Thread and MSH registrations visible to the linker.
add_library(rtthread OBJECT
    vendor/rt-thread/src/clock.c vendor/rt-thread/src/components.c
    vendor/rt-thread/src/idle.c vendor/rt-thread/src/ipc.c vendor/rt-thread/src/irq.c
    vendor/rt-thread/src/kservice.c vendor/rt-thread/src/mem.c
    vendor/rt-thread/src/object.c vendor/rt-thread/src/scheduler.c
    vendor/rt-thread/src/thread.c vendor/rt-thread/src/timer.c
    vendor/rt-thread/cortex-m7/context_gcc.S vendor/rt-thread/cortex-m7/cpuport.c
    vendor/rt-thread/finsh/cmd.c vendor/rt-thread/finsh/msh.c
    vendor/rt-thread/finsh/shell.c
)
set_target_properties(rtthread PROPERTIES C_EXTENSIONS ON)
target_link_libraries(rtthread PRIVATE board_config)

add_library(stm32 OBJECT
    vendor/cmsis/Device/startup_stm32h743xx.S
    vendor/cmsis/Device/system_stm32h7xx.c
    vendor/hal/Src/stm32h7xx_hal.c
    vendor/hal/Src/stm32h7xx_hal_cortex.c
    vendor/hal/Src/stm32h7xx_hal_dma.c
    vendor/hal/Src/stm32h7xx_hal_dma_ex.c
    vendor/hal/Src/stm32h7xx_hal_fdcan.c
    vendor/hal/Src/stm32h7xx_hal_gpio.c
    vendor/hal/Src/stm32h7xx_hal_mdma.c
    vendor/hal/Src/stm32h7xx_hal_pwr.c
    vendor/hal/Src/stm32h7xx_hal_pwr_ex.c
    vendor/hal/Src/stm32h7xx_hal_rcc.c
    vendor/hal/Src/stm32h7xx_hal_rcc_ex.c
    vendor/hal/Src/stm32h7xx_hal_sdram.c
    vendor/hal/Src/stm32h7xx_hal_uart.c
    vendor/hal/Src/stm32h7xx_hal_uart_ex.c
    vendor/hal/Src/stm32h7xx_ll_fmc.c
)
target_link_libraries(stm32 PRIVATE board_config)

add_executable(iot-shark.elf
    board/board.c board/console.c board/fmc.c board/fdcan.c
    app/main.c
)
target_link_libraries(iot-shark.elf PRIVATE board_config shark_diagnostics rtthread stm32)
target_compile_options(iot-shark.elf PRIVATE -Wall -Wextra -Werror)
target_compile_options(shark_diagnostics PRIVATE -ffunction-sections -fdata-sections)

set(linker_script "${CMAKE_CURRENT_SOURCE_DIR}/board/stm32h743.ld")
set_target_properties(iot-shark.elf PROPERTIES LINK_DEPENDS "${linker_script}")
target_link_options(iot-shark.elf PRIVATE
    -nostartfiles --specs=nano.specs --specs=nosys.specs
    -T "${linker_script}"
    "LINKER:--gc-sections,-Map,${CMAKE_CURRENT_BINARY_DIR}/iot-shark.map"
)

add_custom_command(TARGET iot-shark.elf POST_BUILD
    BYPRODUCTS iot-shark.hex iot-shark.bin iot-shark.map
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:iot-shark.elf>" iot-shark.hex
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:iot-shark.elf>" iot-shark.bin
    COMMAND "${CMAKE_COMMAND}"
        "-Delf=$<TARGET_FILE:iot-shark.elf>"
        "-Dbin=${CMAKE_CURRENT_BINARY_DIR}/iot-shark.bin"
        "-Dnm=${CMAKE_NM}"
        "-Dobjdump=${CMAKE_OBJDUMP}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify-firmware.cmake"
    COMMAND "${ARM_size}" "$<TARGET_FILE:iot-shark.elf>"
    VERBATIM
)
