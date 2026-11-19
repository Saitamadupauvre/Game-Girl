# Host-only hooks such as usb_task_reset_test() and fault_reset_test(); never defined in firmware builds
add_compile_definitions(GAME_GIRL_HOST_TEST)

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

add_executable(gamegirl-fault-tests
    test_fault.c
    ../src/services/fault.c
)

target_include_directories(gamegirl-fault-tests PRIVATE ../include)
target_compile_features(gamegirl-fault-tests PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-fault-tests)

add_test(NAME gamegirl-fault-tests COMMAND gamegirl-fault-tests)

# usb.c and hardware_clock.c are not linked: test_usb_task.c fakes the functions usb_task.c calls
add_executable(gamegirl-usb-task-tests
    test_usb_task.c
    ../src/services/usb_task.c
    ../src/services/fault.c
    ../src/common/timer_api.c
)

target_include_directories(gamegirl-usb-task-tests PRIVATE ../include)
target_compile_features(gamegirl-usb-task-tests PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-usb-task-tests)

add_test(NAME gamegirl-usb-task-tests COMMAND gamegirl-usb-task-tests)
