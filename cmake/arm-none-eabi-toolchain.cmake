set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# The firmware cannot be linked until the project supplies its startup code and
# linker script, so do not require a linkable test executable during configure.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
