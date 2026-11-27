enable_language(ASM)

# Passed to both compile and link: the link step uses them to pick the Cortex-M33 soft-float newlib
set(GAMEGIRL_MCU_CPU_FLAGS -mcpu=cortex-m33 -mthumb)
set(GAMEGIRL_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/src/startup/stm32u575zi_linker.ld)

add_executable(gamegirl-firmware
    src/startup/startup_STM32U575xx.S
    src/main.c
    src/drivers/hardware_clock.c
    src/drivers/gpio.c
    src/drivers/timer.c
    src/drivers/watchdog.c
    src/drivers/backup.c
    src/common/timer_api.c
    src/services/inputs.c
    src/services/input_task.c
    src/drivers/usb.c
    src/services/usb_task.c
    src/services/hid.c
    src/services/led.c
    src/services/led_task.c
    src/services/fault.c
    src/services/reset_guard.c
)

set_target_properties(gamegirl-firmware PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS ${GAMEGIRL_LINKER_SCRIPT}     # Relink when only the linker script changes
)

target_include_directories(gamegirl-firmware PRIVATE include)
target_compile_features(gamegirl-firmware PRIVATE c_std_17)
target_compile_options(gamegirl-firmware PRIVATE
    ${GAMEGIRL_MCU_CPU_FLAGS}
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding -fdata-sections -ffunction-sections>
)
gamegirl_enable_warnings(gamegirl-firmware)

# Startup is in startup_STM32U575xx.S, so no crt0. nano.specs links newlib-nano for the
# memcpy/memset calls GCC may emit
target_link_options(gamegirl-firmware PRIVATE
    ${GAMEGIRL_MCU_CPU_FLAGS}
    -T${GAMEGIRL_LINKER_SCRIPT}
    -nostartfiles
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=$<TARGET_FILE_DIR:gamegirl-firmware>/gamegirl-firmware.map
)

add_custom_command(TARGET gamegirl-firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:gamegirl-firmware> $<TARGET_FILE_DIR:gamegirl-firmware>/gamegirl-firmware.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:gamegirl-firmware>
    BYPRODUCTS gamegirl-firmware.bin gamegirl-firmware.map
    VERBATIM
)
