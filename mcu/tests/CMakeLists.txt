add_executable(gamegirl-input-tests
    test_inputs.c
    ../src/drivers/gpio.c
    ../src/services/inputs.c
)

target_include_directories(gamegirl-input-tests PRIVATE ../include)
target_compile_features(gamegirl-input-tests PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-input-tests)

add_test(NAME gamegirl-input-tests COMMAND gamegirl-input-tests)
