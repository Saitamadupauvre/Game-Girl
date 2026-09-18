# Game GIRL File Tree

Recommended monorepo layout, separating STM32 firmware, Raspberry Pi software, and the emulator:

```text
Game-Girl/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .gitignore
├── cmake/
│   ├── arm-none-eabi-toolchain.cmake
│   └── host-toolchain.cmake
│
├── docs/
│   ├── requirements/
│   │   └── cahier-des-charges.md
│   ├── hardware/
│   │   ├── wiring.md
│   │   ├── pinout.md
│   │   ├── power.md
│   │   └── max17043.md
│   ├── software/
│   │   ├── usb-protocol.md
│   │   └── emulator-architecture.md
│   └── testing/
│       ├── display-benchmark.md
│       ├── power-test-plan.md
│       └── acceptance-checklist.md
│
├── shared/
│   ├── include/
│   │   ├── gamegirl_protocol.h
│   │   ├── input_state.h
│   │   └── status.h
│   └── protocol/
│       └── protocol.md
│
├── mcu/
│   ├── CMakeLists.txt
│   ├── startup/
│   │   ├── startup_stm32u575.s
│   │   └── vectors.c
│   ├── linker/
│   │   └── gamegirl.ld
│   ├── include/
│   │   ├── board.h
│   │   ├── clock.h
│   │   ├── gpio.h
│   │   ├── i2c.h
│   │   ├── timer.h
│   │   ├── usb.h
│   │   ├── buttons.h
│   │   ├── battery.h
│   │   ├── power.h
│   │   ├── led.h
│   │   └── rumble.h
│   ├── drivers/
│   │   ├── clock.c
│   │   ├── gpio.c
│   │   ├── i2c.c
│   │   ├── timer.c
│   │   ├── usb/
│   │   │   ├── usb_device.c
│   │   │   ├── usb_hid.c
│   │   │   └── hid_gamepad_report.h
│   │   └── max17043.c
│   ├── services/
│   │   ├── buttons.c
│   │   ├── battery.c
│   │   ├── power.c
│   │   ├── led.c
│   │   ├── rumble.c
│   │   └── protocol.c
│   ├── src/
│   │   └── main.c
│   └── tests/
│       ├── test_buttons.c
│       └── test_protocol.c
│
├── pi/
│   ├── launcher/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.c
│   │   │   ├── ui.c
│   │   │   ├── rom_browser.c
│   │   │   ├── input.c
│   │   │   ├── battery.c
│   │   │   ├── settings.c
│   │   │   └── shutdown.c
│   │   ├── include/
│   │   └── assets/
│   │       ├── logo/
│   │       ├── fonts/
│   │       └── icons/
│   ├── system/
│   │   ├── gamegirl-launcher.service
│   │   ├── gamegirl-shutdown.service
│   │   ├── config/
│   │   │   └── gamegirl.conf
│   │   └── scripts/
│   │       ├── install.sh
│   │       └── setup-display.sh
│   └── tests/
│
├── emulator/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── gba.h
│   │   ├── cpu.h
│   │   ├── bus.h
│   │   ├── ppu.h
│   │   ├── apu.h
│   │   ├── dma.h
│   │   ├── timers.h
│   │   ├── interrupts.h
│   │   ├── keypad.h
│   │   ├── cartridge.h
│   │   └── save.h
│   ├── src/
│   │   ├── main.c
│   │   ├── gba.c
│   │   ├── cpu/
│   │   │   ├── cpu.c
│   │   │   ├── arm.c
│   │   │   └── thumb.c
│   │   ├── memory/
│   │   │   ├── bus.c
│   │   │   └── io.c
│   │   ├── video/
│   │   │   ├── ppu.c
│   │   │   └── renderer.c
│   │   ├── audio/
│   │   │   ├── apu.c
│   │   │   └── audio_output.c
│   │   ├── peripherals/
│   │   │   ├── dma.c
│   │   │   ├── timers.c
│   │   │   ├── interrupts.c
│   │   │   └── keypad.c
│   │   └── cartridge/
│   │       ├── cartridge.c
│   │       └── save.c
│   └── tests/
│       ├── cpu/
│       ├── memory/
│       └── roms/              # Legal test or homebrew ROMs only
│
├── hardware/
│   ├── schematics/
│   ├── pinout/
│   ├── enclosure/
│   ├── bom/
│   │   └── bom.csv
│   └── test-fixtures/
│
├── scripts/
│   ├── build-mcu.sh
│   ├── flash-mcu.sh
│   ├── build-pi.sh
│   └── deploy-pi.sh
│
└── .github/
    └── workflows/
        ├── firmware.yml
        ├── emulator-tests.yml
        └── launcher.yml
```

Create `mcu/`, `shared/`, and `pi/launcher/` first. Keep `emulator/` isolated so it can be tested on a host machine independently from the Raspberry Pi.
