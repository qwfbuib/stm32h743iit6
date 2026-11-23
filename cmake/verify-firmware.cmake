
execute_process(COMMAND "${objdump}" -h "${elf}"
    OUTPUT_VARIABLE sections COMMAND_ERROR_IS_FATAL ANY)
if(NOT sections MATCHES "[.]isr_vector[ \t]+[0-9a-fA-F]+[ \t]+08000000")
    message(FATAL_ERROR "The vector table is missing from 0x08000000")
endif()

execute_process(COMMAND "${nm}" -n --defined-only "${elf}"
    OUTPUT_VARIABLE symbols COMMAND_ERROR_IS_FATAL ANY)
foreach(symbol IN ITEMS Reset_Handler entry PendSV_Handler SysTick_Handler
    USART2_IRQHandler rt_hw_board_init __rt_init_start __rt_init_end
    __rt_init_finsh_system_init __rt_init_board_runtime_init
    __fsymtab_start __fsymtab_end __fsym_sdram __fsym_cantx __fsym_can_rx __fsym_can_selftest)
    if(NOT symbols MATCHES "[0-9a-fA-F]+ [a-zA-Z] ${symbol}[\r\n]")
        message(FATAL_ERROR "Firmware registration or startup symbol missing: ${symbol}")
    endif()
endforeach()

file(READ "${bin}" vectors OFFSET 0 LIMIT 8 HEX)
string(REGEX MATCH "^([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])$" vector_match "${vectors}")
if(NOT vector_match)
    message(FATAL_ERROR "The firmware binary lacks the reset vectors")
endif()
math(EXPR initial_stack "0x${CMAKE_MATCH_4}${CMAKE_MATCH_3}${CMAKE_MATCH_2}${CMAKE_MATCH_1}")
math(EXPR reset_entry "0x${CMAKE_MATCH_8}${CMAKE_MATCH_7}${CMAKE_MATCH_6}${CMAKE_MATCH_5}")
math(EXPR thumb_bit "${reset_entry} & 1")
if(initial_stack LESS_EQUAL 0x24000000 OR initial_stack GREATER 0x24080000
   OR reset_entry LESS 0x08000000 OR reset_entry GREATER_EQUAL 0x08200000
   OR NOT thumb_bit)
    message(FATAL_ERROR "Invalid initial stack pointer or Thumb reset vector")
endif()
message(STATUS "Verified vector table, reset vector, RT-Thread registration, and MSH commands")
