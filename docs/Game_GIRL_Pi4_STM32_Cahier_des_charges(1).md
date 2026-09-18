# Game GIRL — Cahier des charges

## Version

**Version:** V1  
**Architecture:** Raspberry Pi 4 Model B + STM32 Nucleo  
**Primary use cases:** GBA emulation, custom launcher, embedded Linux, bare-metal STM32 firmware  
**Optional use case:** Moonlight game streaming  
**Document type:** Cahier des charges / system specification

---

# 1. Project objective

Game GIRL is a custom handheld console built as a student embedded-systems project.

The goal is to combine:

- a **Raspberry Pi 4 Model B** for Linux, graphics, storage and GBA emulation;
- an **STM32U575** on a **NUCLEO-U575ZI-Q** development board for bare-metal firmware;
- a **3.5-inch non-touch SPI display**;
- a **single-cell LiPo battery**;
- physical controls;
- battery telemetry;
- audio output;
- removable microSD storage;
- a custom enclosure.

The project should demonstrate both:

```text
embedded Linux
+
bare-metal firmware
+
hardware integration
+
emulator development
```

The final device should boot directly into a Game GIRL interface rather than a normal desktop environment.

---

# 2. High-level architecture

```text
                         GAME GIRL
                              │
              ┌───────────────┴───────────────┐
              │                               │
       Raspberry Pi 4B                  STM32U575
          Linux side                   bare-metal side
              │                               │
      ┌───────┼──────────┐          ┌─────────┼─────────┐
      │       │          │          │         │         │
   Display   Audio      Storage    Buttons   Battery   Power
      │       │          │          │         │         │
      │       │          │          │         │         │
      └───────┴──────────┴──────────┴─────────┴─────────┘
                              │
                           User
```

The Raspberry Pi handles the computationally heavy tasks.

The STM32 handles physical-console responsibilities and provides firmware experience.

---

# 3. Main hardware

## 3.1 Raspberry Pi

**Selected platform:**

```text
Raspberry Pi 4 Model B
RAM: 2 GB
Storage: microSD only
Wi-Fi: integrated
Bluetooth: integrated
```

Responsibilities:

- boot Linux;
- run Game GIRL launcher;
- run custom GBA emulator;
- optionally run Moonlight;
- render graphics;
- handle audio;
- manage filesystems;
- read ROMs and saves;
- communicate with STM32 over USB.

The Raspberry Pi is not intended to expose a normal desktop in the final system.

---

## 3.2 STM32

**Selected platform:**

```text
ST NUCLEO-U575ZI-Q
MCU: STM32U575
Core: Arm Cortex-M33
```

The Nucleo board will remain intact inside the V1 prototype.

Responsibilities:

- scan buttons;
- debounce inputs;
- expose controls as USB HID;
- read battery state;
- communicate with fuel gauge;
- handle power button;
- generate low-battery alerts;
- control LEDs;
- control rumble;
- coordinate safe shutdown;
- provide firmware-version and hardware-status information.

The STM32 firmware should be written without STM32 HAL where practical.

---

# 4. Display

## 4.1 Selected screen

**Elecrow DLS24035B**

Specifications:

```text
Size:          3.5 inch
Resolution:    480 × 320
Controller:    ILI9488
Interface:     4-wire SPI
Touch:         none
Logic:         3.3 V
Supply:        3.3–5 V
```

This display is selected for both:

- prototype;
- V1 enclosure.

The screen is considered **locked** unless integration testing shows unacceptable refresh performance.

---

## 4.2 Display role

The screen will display:

- boot logo;
- Game GIRL launcher;
- GBA framebuffer;
- battery status;
- settings;
- optional Moonlight stream.

The GBA native resolution is:

```text
240 × 160
```

The selected display is:

```text
480 × 320
```

which provides exact 2× integer scaling.

---

# 5. Storage

## 5.1 Main storage

V1 will use a single microSD card.

Target capacity:

```text
64 GB
```

Suggested layout:

```text
/
├── boot/
├── Linux
├── gamegirl/
│   ├── launcher
│   ├── emulator
│   ├── config
│   └── assets
│
├── roms/
│   └── gba/
│       ├── PokemonEmerald.gba
│       ├── PokemonFireRed.gba
│       └── homebrew.gba
│
└── saves/
    ├── PokemonEmerald.sav
    └── PokemonFireRed.sav
```

No eMMC is required in V1.

---

# 6. Controls

## 6.1 Physical controls

Target control set:

```text
D-pad
├── Up
├── Down
├── Left
└── Right

Face buttons
├── A
├── B
├── X
└── Y

Shoulders
├── L
└── R

System buttons
├── Start
├── Select
└── Menu
```

Prototype controls use standard 6×6 mm momentary tactile switches.

The current button kit is acceptable for prototyping because it provides multiple actuator heights.

---

## 6.2 Electrical connection

Each button should use a simple active-low input:

```text
STM32 GPIO
    │
    │
  switch
    │
   GND
```

The STM32 enables an internal pull-up.

Logical state:

```text
HIGH → released
LOW  → pressed
```

---

## 6.3 Input representation

Recommended firmware representation:

```c
typedef struct {
    uint16_t buttons;
} input_state_t;
```

Example bit allocation:

```c
enum {
    BTN_A      = 1u << 0,
    BTN_B      = 1u << 1,
    BTN_SELECT = 1u << 2,
    BTN_START  = 1u << 3,
    BTN_RIGHT  = 1u << 4,
    BTN_LEFT   = 1u << 5,
    BTN_UP     = 1u << 6,
    BTN_DOWN   = 1u << 7,
    BTN_R      = 1u << 8,
    BTN_L      = 1u << 9,
    BTN_MENU   = 1u << 10,
    BTN_X      = 1u << 11,
    BTN_Y      = 1u << 12,
};
```

---

## 6.4 Debouncing

Mechanical switches bounce electrically.

Firmware must debounce inputs.

Target debounce interval:

```text
5–10 ms
```

Possible strategy:

```text
read input
   ↓
state changed?
   ↓
start debounce timer
   ↓
stable for 5–10 ms?
   ↓
accept new state
```

---

# 7. STM32 ↔ Raspberry Pi communication

## 7.1 Primary interface

Preferred interface:

```text
USB
```

The STM32 should enumerate as a standard **USB HID gamepad**.

Linux then sees an input device such as:

```text
/dev/input/eventX
```

Advantages:

- standard Linux input stack;
- emulator compatibility;
- Moonlight compatibility;
- no custom kernel driver needed;
- easy debugging.

---

## 7.2 Optional vendor-specific protocol

A second control channel may be added over USB.

Example commands:

```text
CMD_GET_BATTERY
CMD_GET_CHARGER_STATE
CMD_SET_RUMBLE
CMD_SET_LED
CMD_GET_FW_VERSION
CMD_GET_HW_VERSION
CMD_REQUEST_SHUTDOWN
```

Example status structure:

```c
typedef struct {
    uint16_t buttons;
    uint16_t battery_mv;
    uint8_t  battery_percent;
    uint8_t  charging;
    uint8_t  flags;
} gamegirl_status_t;
```

---

# 8. Battery system

## 8.1 Battery

Target battery:

```text
Chemistry:      1S LiPo
Nominal voltage: 3.7 V
Full voltage:    4.2 V
Capacity target: ~5000 mAh
```

The final physical cell must be selected after enclosure dimensions are known.

The battery should be capable of several amps of discharge current.

---

# 9. Main 5 V rail

The Raspberry Pi requires a regulated 5 V supply.

The battery voltage is only approximately:

```text
3.0–4.2 V
```

Therefore V1 requires a boost converter.

Target topology:

```text
1S LiPo
   │
   ▼
high-current boost converter
   │
   ▼
5.0 V rail
   │
   ├── Raspberry Pi 4
   ├── STM32 Nucleo
   ├── LCD
   └── audio amplifier
```

---

## 9.1 Selected boost-converter class

Current candidate:

```text
TPS61088-based boost module
Input: approximately 2.7–12 V
Output: configurable, set to 5.0 V
Power class: approximately 30 W
```

Target operating requirement:

```text
5 V
≈4 A continuous design target
```

The module must be load-tested before integration.

---

## 9.2 Battery-side current

Because voltage is boosted, battery current is higher than output current.

Example:

```text
20 W output
battery voltage ≈ 3.7 V
efficiency ≈ 90%
```

Battery current is approximately:

```text
20 / (3.7 × 0.90)
≈ 6 A
```

Therefore:

- battery;
- wiring;
- connector;
- charger;
- boost module

must all be chosen for high-current operation.

---

# 10. Fuel gauge

## 10.1 Selected family

Current prototype candidate:

```text
MAX17043 / MAX17048
```

The current Amazon module is based on **MAX17043**.

Role:

- measure cell voltage;
- estimate state of charge;
- provide low-battery alert;
- report battery percentage.

Communication:

```text
I²C
```

Connection:

```text
LiPo
 │
 ▼
MAX17043
 │
 │ I²C
 ▼
STM32
```

---

## 10.2 I²C pull-up requirement

The STM32 uses 3.3 V logic.

The SDA and SCL pull-ups should therefore ideally be:

```text
3.3 V
```

Before connecting the module to the STM32, measure SDA and SCL with a multimeter.

Expected:

```text
~3.3 V
```

If the lines rise to battery voltage:

```text
~4.2 V
```

the breakout must be modified or replaced before connection.

---

## 10.3 Battery polarity check

Before plugging the LiPo into the fuel gauge:

```text
red probe   → expected +
black probe → expected -
```

Expected multimeter result:

```text
positive voltage → correct
negative voltage → reversed polarity
```

Never assume JST polarity from connector shape alone.

---

# 11. Charging

The V1 charger should be an off-the-shelf module.

Requirements:

- 1S LiPo;
- USB-C input preferred;
- battery protection;
- over-current protection;
- thermal protection preferred;
- ability to operate while system is powered preferred;
- power-path / load-sharing strongly preferred.

A basic charger-only board is not ideal if Game GIRL must be playable while charging.

Exact charger module remains **TBD**.

---

# 12. Audio

## 12.1 Amplifier

Selected class:

```text
MAX98357A
```

Architecture:

```text
Raspberry Pi
   │
   │ I²S
   ▼
MAX98357A
   │
   ▼
4 Ω speaker
```

V1 uses mono audio.

---

## 12.2 Speaker

Target:

```text
Impedance: 4 Ω
Power:     ~3 W
Diameter:  ~28–40 mm
```

Final size depends on enclosure geometry.

---

# 13. Rumble

Optional V1 feature.

Architecture:

```text
STM32 GPIO
    │
    ▼
MOSFET driver
    │
    ▼
vibration motor
```

The motor must never be driven directly from an STM32 GPIO.

---

# 14. LEDs

Possible LEDs:

- power;
- charging;
- low battery;
- status/debug.

LEDs are controlled by STM32 GPIO or PWM.

---

# 15. Raspberry Pi software stack

Target production stack:

```text
Boot ROM
   ↓
Raspberry Pi bootloader
   ↓
Linux kernel
   ↓
minimal root filesystem
   ↓
Game GIRL launcher
   │
   ├── GBA emulator
   ├── Moonlight optional
   └── Settings
```

The final system should not boot into a normal desktop.

---

# 16. Linux strategy

Development phase:

```text
Raspberry Pi OS Lite
```

Possible later production image:

- Buildroot;
- Yocto;
- stripped Raspberry Pi OS.

Unnecessary services should be removed where practical.

---

# 17. Game GIRL launcher

Target UI:

```text
┌──────────────────────────┐
│        GAME GIRL         │
│                          │
│   > GBA Games            │
│     Moonlight            │
│     Settings             │
│                          │
│ Battery: 78%             │
└──────────────────────────┘
```

Responsibilities:

- list ROMs;
- launch emulator;
- show battery status;
- launch Moonlight if enabled;
- settings;
- shutdown;
- return from emulator to menu.

---

# 18. GBA emulator

The emulator is custom software.

Major subsystems:

```text
GBA Emulator
├── ARM7TDMI CPU
│   ├── ARM instruction set
│   └── Thumb instruction set
├── memory map
├── memory-mapped I/O
├── DMA
├── timers
├── interrupts
├── PPU
├── keypad
├── audio
├── cartridge interface
└── save hardware
```

---

# 19. Emulator CPU core

Start with an interpreter.

Pipeline:

```text
fetch
 ↓
decode
 ↓
execute
 ↓
update flags
 ↓
timing
```

Prioritize correctness before optimization.

---

# 20. Emulator memory map

Approximate GBA map:

```text
0x00000000  BIOS
0x02000000  EWRAM
0x03000000  IWRAM
0x04000000  I/O
0x05000000  Palette RAM
0x06000000  VRAM
0x07000000  OAM
0x08000000  Game Pak ROM
0x0E000000  Game Pak save memory
```

Recommended bus API:

```c
uint8_t  bus_read8(uint32_t addr);
uint16_t bus_read16(uint32_t addr);
uint32_t bus_read32(uint32_t addr);

void bus_write8(uint32_t addr, uint8_t value);
void bus_write16(uint32_t addr, uint16_t value);
void bus_write32(uint32_t addr, uint32_t value);
```

---

# 21. Graphics path

```text
GBA PPU
  │
  ▼
240×160 framebuffer
  │
  ▼
2× scaling
  │
  ▼
480×320
  │
  ▼
Linux framebuffer / DRM / SPI display path
  │
  ▼
ILI9488 LCD
```

The SPI screen is a potential bandwidth bottleneck.

This must be benchmarked early.

---

# 22. Audio path

```text
GBA APU
  │
  ▼
software mixer
  │
  ▼
PCM samples
  │
  ▼
Linux audio buffer
  │
  ▼
I²S
  │
  ▼
MAX98357A
```

---

# 23. STM32 firmware philosophy

The STM32 firmware should demonstrate low-level embedded work.

Preferred tools:

```text
arm-none-eabi-gcc
C17
CMake
Ninja
OpenOCD
GDB
ST-LINK
```

Target principles:

- custom startup;
- custom linker script;
- direct register access where practical;
- no STM32 HAL in core firmware;
- no RTOS initially.

---

# 24. STM32 firmware repository

```text
mcu/
├── startup/
│   ├── startup.s
│   └── vectors.c
│
├── linker/
│   └── gamegirl.ld
│
├── drivers/
│   ├── clock.c
│   ├── gpio.c
│   ├── timer.c
│   ├── i2c.c
│   ├── adc.c
│   ├── pwm.c
│   └── usb.c
│
├── services/
│   ├── buttons.c
│   ├── battery.c
│   ├── power.c
│   ├── rumble.c
│   └── protocol.c
│
└── main.c
```

---

# 25. STM32 boot sequence

```text
RESET
  ↓
Reset_Handler
  ↓
initialize memory
  ↓
clock_init()
  ↓
gpio_init()
  ↓
timer_init()
  ↓
i2c_init()
  ↓
usb_init()
  ↓
main loop
```

---

# 26. STM32 main loop

Conceptually:

```text
scan buttons
    ↓
debounce
    ↓
update USB HID report
    ↓
read battery
    ↓
check power button
    ↓
process host commands
    ↓
sleep / repeat
```

---

# 27. Flashing STM32 firmware

The Nucleo board includes ST-LINK.

Build flow:

```text
C source
   ↓
arm-none-eabi-gcc
   ↓
gamegirl.elf
   ↓
OpenOCD
   ↓
ST-LINK
   ↓
STM32 flash
```

Recommended build targets:

```bash
cmake --build build
cmake --build build --target flash
cmake --build build --target reset
cmake --build build --target erase
```

---

# 28. Example CMake flash target

```cmake
add_custom_target(flash
    COMMAND openocd
        -f interface/stlink.cfg
        -f target/stm32u5x.cfg
        -c "program $<TARGET_FILE:gamegirl> verify reset exit"
    DEPENDS gamegirl
)
```

Exact OpenOCD target filename should be verified against the installed OpenOCD version.

---

# 29. Safe shutdown

The STM32 should coordinate shutdown.

Desired sequence:

```text
Power button held
       ↓
STM32 detects request
       ↓
STM32 sends shutdown command
       ↓
Linux flushes saves
       ↓
Linux syncs filesystem
       ↓
Linux shuts down
       ↓
main 5 V rail may be disabled
```

For the first prototype, physical power cutoff may remain manual.

Later revisions can add an MCU-controlled load switch.

---

# 30. Boot behavior

Target user experience:

```text
Power on
   ↓
Pi boots
   ↓
Game GIRL logo
   ↓
Game GIRL launcher
```

The normal Linux console should remain hidden from the user.

---

# 31. Enclosure

V1 enclosure:

- 3D printed;
- PLA/PETG prototype material;
- designed around full Raspberry Pi 4B;
- designed around full Nucleo board;
- removable back preferred;
- access to SD card;
- access to Pi USB-C power during development;
- access to ST-LINK USB connector;
- ventilation around Pi.

---

# 32. Mechanical constraints

The enclosure must accommodate:

- Pi 4 board;
- Nucleo board;
- 3.5-inch LCD;
- LiPo;
- speaker;
- buttons;
- boost converter;
- charger module;
- wiring;
- fuel gauge.

V1 is allowed to be relatively thick.

Miniaturization is not the primary goal.

---

# 33. Thermal requirements

The Raspberry Pi 4 can become warm under sustained load.

Prototype should include:

- passive heatsink;
- ventilation;
- temperature monitoring.

A fan should only be added if measurements show it is required.

---

# 34. Prototype hardware list

Current V1 components:

```text
Raspberry Pi 4 Model B 2 GB
NUCLEO-U575ZI-Q
Elecrow DLS24035B 3.5" ILI9488 display
64 GB microSD
MAX98357A audio breakout
4 Ω / ~3 W speaker
1S ~5000 mAh LiPo
TPS61088-class 30 W boost module
MAX17043 fuel-gauge module
1S USB-C charger / power-path module
tactile switch kit
breadboard / perfboard
jumper wires
passive heatsink
3D-printed shell
```

---

# 35. Development phases

## Phase 1 — Raspberry Pi bring-up

Tasks:

- install Raspberry Pi OS Lite;
- boot from microSD;
- configure SSH;
- configure Wi-Fi;
- verify GPIO/SPI/I²S;
- establish development environment.

Acceptance:

```text
Pi boots reliably
SSH works
storage works
```

---

## Phase 2 — STM32 bring-up

Tasks:

- install ARM GCC;
- write startup code;
- write linker script;
- UART debug;
- GPIO;
- timers.

Acceptance:

```text
STM32 boots custom bare-metal firmware
```

---

## Phase 3 — Controls

Tasks:

- wire tactile switches;
- implement debounce;
- implement input bitmask;
- USB HID gamepad.

Acceptance:

```text
Linux recognizes STM32 as gamepad
all controls work
```

---

## Phase 4 — Display

Tasks:

- connect Elecrow ILI9488;
- configure SPI;
- render test image;
- measure frame-update speed.

Acceptance:

```text
480×320 image displayed reliably
```

---

## Phase 5 — Audio

Tasks:

- configure I²S;
- connect MAX98357A;
- play test tone;
- play PCM sample.

Acceptance:

```text
clean mono audio
```

---

## Phase 6 — Battery telemetry

Tasks:

- connect MAX17043;
- verify I²C pull-up voltage;
- verify JST polarity;
- implement STM32 I²C driver;
- display battery percentage on Pi.

Acceptance:

```text
battery voltage and percentage visible
```

---

## Phase 7 — Portable power

Tasks:

- charger;
- battery;
- TPS61088 boost;
- load test;
- thermal test;
- brownout test.

Acceptance:

```text
console runs from battery without undervoltage
```

---

## Phase 8 — Launcher

Tasks:

- direct boot;
- ROM browser;
- settings;
- battery display;
- shutdown.

Acceptance:

```text
no desktop required
```

---

## Phase 9 — GBA emulator

Tasks:

- ARM7TDMI;
- Thumb;
- memory;
- timers;
- interrupts;
- DMA;
- PPU;
- keypad;
- audio;
- saves.

Acceptance:

```text
test ROMs pass
homebrew becomes playable
```

---

## Phase 10 — Pokémon target

Final compatibility target:

```text
selected legally dumped Pokémon GBA ROM
```

Acceptance:

- boots;
- graphics render correctly;
- controls work;
- saves work;
- audio works;
- stable gameplay.

---

# 36. Optional Moonlight phase

Moonlight is optional.

If retained:

- Wi-Fi performance must be tested;
- hardware video decode must be used;
- SPI LCD refresh may become the limiting factor.

Moonlight should not block the core GBA milestone.

---

# 37. Acceptance criteria

A V1 Game GIRL is considered successful when:

```text
[ ] Boots from microSD
[ ] Launcher starts automatically
[ ] STM32 runs custom bare-metal firmware
[ ] STM32 enumerates as USB HID
[ ] All physical controls function
[ ] LCD renders launcher and games
[ ] Audio works
[ ] Battery percentage is available
[ ] Runs from LiPo battery
[ ] Safe shutdown exists
[ ] ROM browser works
[ ] Emulator runs GBA software
[ ] Save files persist
[ ] Enclosure contains all required hardware
```

---

# 38. Safety requirements

Electrical safety:

- never connect unknown LiPo polarity directly;
- verify JST polarity with multimeter;
- do not exceed GPIO voltage limits;
- do not drive motors from GPIO directly;
- do not run high-current boost paths through solderless breadboards;
- use short, adequately thick power wiring;
- test boost converter before connecting Pi;
- verify 5 V rail before powering electronics;
- monitor battery temperature during early power tests.

---

# 39. Main risks

## Risk: SPI display bandwidth

Mitigation:

- benchmark early;
- use optimized Linux driver;
- reduce unnecessary full-screen updates;
- consider alternative display interface in future revision.

## Risk: battery current

Mitigation:

- use high-discharge-capable LiPo;
- size wiring correctly;
- load-test power stage.

## Risk: boost converter thermal performance

Mitigation:

- sustained-load testing;
- heatsinking if necessary;
- use conservative current target.

## Risk: Linux shutdown corruption

Mitigation:

- STM32-coordinated shutdown;
- read-only filesystem options later;
- explicit save flushing.

## Risk: project scope

Mitigation:

Core milestone order:

```text
controls
→ display
→ audio
→ battery
→ launcher
→ emulator
→ optional Moonlight
```

Moonlight is not required for first success.

---

# 40. Future V2 improvements

Possible future improvements:

- replace Pi 4B with CM4;
- replace full Nucleo board with custom STM32 PCB;
- custom carrier board;
- smaller battery-management PCB;
- silicone membrane controls;
- stereo audio;
- dedicated power latch;
- sleep/wake support;
- better display interface;
- smaller enclosure;
- custom PCB integration.

---

# 41. Final V1 block diagram

```text
                            GAME GIRL V1

             ┌─────────────────────────────────┐
             │       Raspberry Pi 4B 2GB       │
             │                                 │
             │  Linux                          │
             │  ├── Game GIRL launcher         │
             │  ├── GBA emulator               │
             │  └── Moonlight optional         │
             │                                 │
             ├── SPI ─────► ILI9488 LCD        │
             ├── I²S ─────► MAX98357A          │
             ├── microSD ─► OS / ROMs / saves  │
             └── USB ◄────► STM32U575          │
             └────────────────┬────────────────┘
                              │
                        USB HID + status
                              │
             ┌────────────────▼────────────────┐
             │       NUCLEO-U575ZI-Q           │
             │                                 │
             │  bare-metal firmware            │
             │  ├── buttons                    │
             │  ├── debounce                   │
             │  ├── fuel gauge                 │
             │  ├── power button               │
             │  ├── rumble                     │
             │  └── LEDs                       │
             └────────────────┬────────────────┘
                              │
                              ▼
                          MAX17043
                              │
                            LiPo

LiPo ──► charger/power-path ──► TPS61088-class boost ──► 5 V system rail
                                                    │
                                                    ├── Pi 4
                                                    ├── Nucleo
                                                    ├── LCD
                                                    └── audio
```

---

# 42. Project positioning

Game GIRL V1 should demonstrate:

- bare-metal ARM Cortex-M firmware;
- USB HID;
- I²C device integration;
- power-system integration;
- embedded Linux;
- Linux hardware interfaces;
- graphics/audio pipelines;
- emulator architecture;
- ARM7TDMI emulation;
- system integration;
- hardware debugging;
- enclosure/mechanical prototyping.

The final result should be a functioning handheld console, not only a collection of dev boards.

The modular V1 architecture is intentionally designed to make the project achievable without requiring a custom PCB.
