
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(ARM_GNU_TOOLCHAIN_ROOT "$ENV{ARM_GNU_TOOLCHAIN_ROOT}" CACHE PATH
    "Directory containing the Arm GNU Toolchain bin directory")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_GNU_TOOLCHAIN_ROOT)

foreach(tool IN ITEMS gcc objcopy objdump nm size)
    if(ARM_GNU_TOOLCHAIN_ROOT)
        find_program(ARM_${tool} NAMES arm-none-eabi-${tool}
            PATHS "${ARM_GNU_TOOLCHAIN_ROOT}/bin" NO_DEFAULT_PATH REQUIRED)
    else()
        find_program(ARM_${tool} NAMES arm-none-eabi-${tool} REQUIRED)
    endif()
endforeach()

set(CMAKE_C_COMPILER "${ARM_gcc}")
set(CMAKE_ASM_COMPILER "${ARM_gcc}")
set(CMAKE_OBJCOPY "${ARM_objcopy}")
set(CMAKE_OBJDUMP "${ARM_objdump}")
set(CMAKE_NM "${ARM_nm}")

set(shark_cpu_flags "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT "${shark_cpu_flags}")
set(CMAKE_ASM_FLAGS_INIT "${shark_cpu_flags}")
