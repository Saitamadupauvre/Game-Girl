add_executable(gamegirl-input-tests
    test_inputs.c
    ../src/drivers/gpio.c
    ../src/services/inputs.c
)

target_include_directories(gamegirl-input-tests PRIVATE ../include)
target_compile_features(gamegirl-input-tests PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-input-tests)

add_test(NAME gamegirl-input-tests COMMAND gamegirl-input-tests)

add_executable(gamegirl-hid-tests
    test_hid.c
    ../src/services/hid.c
)

target_include_directories(gamegirl-hid-tests PRIVATE ../include)
target_compile_features(gamegirl-hid-tests PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-hid-tests)

add_test(NAME gamegirl-hid-tests COMMAND gamegirl-hid-tests)

add_executable(gamegirl-timer-tests
    test_timer.c
    ../src/common/timer_api.c
)

target_include_directories(gamegirl-timer-tests PRIVATE ../include)
target_compile_features(gamegirl-timer-tests PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-timer-tests)

add_test(NAME gamegirl-timer-tests COMMAND gamegirl-timer-tests)

add_executable(gamegirl-led-tests
    test_led.c
    ../src/services/led.c
)

target_include_directories(gamegirl-led-tests PRIVATE ../include)
target_compile_features(gamegirl-led-tests PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-led-tests)

add_test(NAME gamegirl-led-tests COMMAND gamegirl-led-tests)
