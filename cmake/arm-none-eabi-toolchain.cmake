set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# CMake's configure-time compiler check has no startup code or linker script, so
# build it as a static library instead of linking an executable.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
