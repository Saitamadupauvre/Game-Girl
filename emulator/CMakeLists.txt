add_executable(gamegirl-emulator
    src/main.c
    src/gba.c
)

target_include_directories(gamegirl-emulator PRIVATE include)
target_compile_features(gamegirl-emulator PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-emulator)
