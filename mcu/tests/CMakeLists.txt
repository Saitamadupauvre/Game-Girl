add_executable(gamegirl-input-tests
    test_inputs.c
    ../src/drivers/gpio.c
    ../src/services/inputs.c
)

target_include_directories(gamegirl-input-tests PRIVATE ../include)
target_compile_features(gamegirl-input-tests PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-input-tests)

add_test(NAME gamegirl-input-tests COMMAND gamegirl-input-tests)

add_executable(gamegirl-led-tests
    test_led.c
    ../src/services/led.c
)

target_include_directories(gamegirl-led-tests PRIVATE ../include)
target_compile_features(gamegirl-led-tests PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-led-tests)

add_test(NAME gamegirl-led-tests COMMAND gamegirl-led-tests)
