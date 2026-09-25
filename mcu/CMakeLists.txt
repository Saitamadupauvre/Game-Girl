# A startup routine and linker script are required before this can produce a
# flashable ELF. Compile the current firmware sources as cross-compiled objects.
add_library(gamegirl-firmware-objects OBJECT
    src/main.c
    src/drivers/gpio.c
)

target_include_directories(gamegirl-firmware-objects PRIVATE include)
target_compile_features(gamegirl-firmware-objects PRIVATE c_std_17)
target_compile_options(gamegirl-firmware-objects PRIVATE
    -mcpu=cortex-m33
    -mthumb
    -ffreestanding
    -fdata-sections
    -ffunction-sections
)
gamegirl_enable_warnings(gamegirl-firmware-objects)

add_custom_target(gamegirl-firmware DEPENDS gamegirl-firmware-objects)
