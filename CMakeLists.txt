cmake_minimum_required(VERSION 3.20)

project(GameGirl LANGUAGES C)

# Lets clangd and other C tooling use each target's CMake include directories.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

option(GAMEGIRL_BUILD_EMULATOR "Build the host GBA emulator" ON)
option(GAMEGIRL_BUILD_LAUNCHER "Build the Raspberry Pi launcher" ON)
option(GAMEGIRL_BUILD_MCU "Build STM32 firmware object files" OFF)

function(gamegirl_enable_warnings target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()

if(GAMEGIRL_BUILD_EMULATOR)
    add_subdirectory(emulator)
endif()

if(GAMEGIRL_BUILD_LAUNCHER)
    add_subdirectory(pi/launcher)
endif()

if(GAMEGIRL_BUILD_MCU)
    add_subdirectory(mcu)
endif()
