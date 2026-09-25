add_executable(gamegirl-launcher
    src/main.c
    src/launcher.c
)

target_include_directories(gamegirl-launcher PRIVATE include)
target_compile_features(gamegirl-launcher PRIVATE c_std_17)
gamegirl_enable_warnings(gamegirl-launcher)
