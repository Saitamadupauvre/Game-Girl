#!/usr/bin/env bash
# Shortcut for the common Game-Girl build commands. Run `./GG.sh help` for usage.
set -euo pipefail

cd "$(dirname "$0")"

HOST_BUILD=build
MCU_BUILD=build-mcu

configure_host() {
    cmake -S . -B "$HOST_BUILD"
}

configure_mcu() {
    cmake -S . -B "$MCU_BUILD" \
        -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake \
        -DGAMEGIRL_BUILD_MCU=ON \
        -DGAMEGIRL_BUILD_EMULATOR=OFF \
        -DGAMEGIRL_BUILD_LAUNCHER=OFF \
        -DBUILD_TESTING=OFF
}

build_host() {
    configure_host
    cmake --build "$HOST_BUILD" -j
}

run_tests() {
    build_host
    ctest --test-dir "$HOST_BUILD" --output-on-failure
}

build_mcu() {
    configure_mcu
    cmake --build "$MCU_BUILD" -j
}

usage() {
    cat <<EOF
Usage: ./GG.sh <command>

  build   Configure and build host targets (emulator, launcher, tests) in $HOST_BUILD/
  test    Build host targets and run the boardless tests with ctest
  mcu     Cross-compile the STM32 firmware objects in $MCU_BUILD/
  all     test + mcu
  clean   Delete $HOST_BUILD/ and $MCU_BUILD/
EOF
}

case "${1:-help}" in
    build) build_host ;;
    test)  run_tests ;;
    mcu)   build_mcu ;;
    all)   run_tests; build_mcu ;;
    clean) rm -rf "$HOST_BUILD" "$MCU_BUILD" ;;
    help|-h|--help) usage ;;
    *) usage; exit 1 ;;
esac
