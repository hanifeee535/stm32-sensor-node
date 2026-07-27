# STM32 Board Customization Procedure

**Repository:** `stm32-sensor-node`

---

## 1. Purpose

This document records the procedure followed for porting a custom Zephyr board definition inside `stm32-sensor-node`, using the sensor node's current target — an STM32F407-based board derived from the STM32F4 Discovery.

It is kept as a reference note for reuse in future projects: the same steps apply to any future board revision (a new PCB spin, a different STM32 part, a second product variant) that starts from an existing, Zephyr-supported reference board.

---

## 2. Zephyr Board Directory Convention

Since Zephyr's board metadata format (`board.yml`), a board is identified by a `<vendor>/<board_name>` pair and lives at:

```
<app-repo>/boards/<vendor>/<board_name>/
```

`west`'s board discovery does **not** scan an application's own `boards/` directory automatically. The application's `CMakeLists.txt` has to add itself to `BOARD_ROOT` explicitly, before `find_package(Zephyr...)`:

```cmake
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
```

Without this line, a correctly written board definition is simply invisible to `west build` — it fails with "No board named ... found" even though every file is correct.

The board target string passed to `west build -b` is the board's `name:` field alone (e.g. `stm32f4_sensor_node`) — **not** `<vendor>/<board_name>`. The vendor segment is directory and metadata organization only; it is not part of the buildable board identifier.

A complete board definition consists of:

| File | Role |
|---|---|
| `board.yml` | Declares the board's identity (`name`, `full_name`, `vendor`) and which SoC it uses. Read by `west` and Zephyr's build system to resolve the board target. |
| `<board_name>.yaml` | Board metadata used by Zephyr's test runner (Twister) and tooling — RAM/flash size, architecture, toolchain support, supported peripheral features. |
| `Kconfig.<board_name>` | Declares the `BOARD_<BOARD_NAME>` Kconfig symbol and selects the underlying SoC Kconfig (`SOC_STM32F407XG`). |
| `<board_name>_defconfig` | Default Kconfig values applied whenever this board is built (console, GPIO, MPU, stack protection, etc.). |
| `<board_name>.dts` | The board's devicetree — pin assignments, enabled peripherals, clock tree, board-level nodes (LEDs, buttons, sensors). This is the file that actually encodes the hardware. |
| `board.cmake` | Declares which flash/debug runners (OpenOCD, J-Link, STM32CubeProgrammer) are available for this board and their arguments. |
| `support/openocd.cfg` | OpenOCD target configuration used by the OpenOCD runner declared in `board.cmake`. |
| `doc/index.rst` | Optional Sphinx documentation page for the board, in Zephyr's own documentation format. |

---

## 3. Procedure Followed

Steps 1–7 below have all been completed for this board — see the status note at the end of [Section 4](#4-worked-example--sensor_nodestm32f4_sensor_node).

### Step 1 — Identifying the Closest Reference Board

A board definition was not written from a blank page. The Zephyr board most closely matching the actual silicon and physical layout was identified first, and its board directory was used as the starting template. For this product, that reference was `zephyr/boards/st/stm32f4_disco/`, since the sensor node's MCU and base layout are electrically an STM32F407 Discovery.

### Step 2 — Creating the Target Directory

The target directory was created following Zephyr's convention:

```
stm32-sensor-node/boards/<vendor>/<board_name>/
```

The `<vendor>` segment does not have to be a registered silicon vendor — it is a free-form namespace string in Zephyr's board schema (validated only as a string, not against a fixed vendor list). It was chosen to identify who owns this board definition, not who manufactures the chip.

### Step 3 — Copying the Reference Board's Files

Every file was copied from the reference board directory into the new one:

```
board.yml
board.cmake
Kconfig.<reference_board_name>
<reference_board_name>_defconfig
<reference_board_name>.dts
<reference_board_name>.yaml
support/openocd.cfg      (if present)
doc/                     (optional)
```

### Step 4 — Renaming Every File to the New Identifier

This was the step most easily missed, and the one most likely to produce a board that silently fails to resolve or resolves to the wrong identity. Copying the files into a renamed directory alone was **not** sufficient — every filename that embedded the old board name had to be renamed individually:

```
Kconfig.<old_name>        → Kconfig.<new_name>
<old_name>_defconfig      → <new_name>_defconfig
<old_name>.dts            → <new_name>.dts
<old_name>.yaml           → <new_name>.yaml
```

`board.yml` and `board.cmake` kept their names — only their contents changed.

### Step 5 — Editing the Contents of Every Renamed File

| File | What changed |
|---|---|
| `board.yml` | `name:` → new board name. `full_name:` → a human-readable product name. `vendor:` → the new vendor string. `socs:` left unchanged, since the SoC was unchanged. |
| `<new_name>.yaml` | `identifier:` and `name:` → matched `board.yml`. `vendor:` → matched `board.yml`. `ram`/`flash`/`toolchain`/`supported` left as inherited. |
| `Kconfig.<new_name>` | The `config BOARD_<OLD_NAME>` symbol was renamed to `config BOARD_<NEW_NAME>` (uppercase, underscored). The `select SOC_...` line was left as-is, since the SoC was unchanged. |
| `<new_name>.dts` | `model` and `compatible` were updated to the new board identity. The rest of the devicetree (clocks, peripherals, board-level nodes) was left as inherited from the reference board at this stage — stripping it down to a minimal baseline is Step 6. |
| `board.cmake` | Left unchanged — the debug/flash interface is the same as the reference board. |
| `support/openocd.cfg` | Left unchanged — the OpenOCD target config is the same as the reference board. |


### Step 6 — Stripping the Devicetree to a Minimal, Testable Baseline

Once the board resolved under its new identity, the devicetree copied in Step 3 still described the *reference* board's full peripheral set (its four LEDs, its pushbutton, PWM LEDs, CAN, ADC, I²S, and its onboard audio codec) — not the sensor node's actual wiring, and far more than needed to prove the port works at all. Rather than adapting all of it at once, everything not required for a first smoke test was removed, and the rest was deferred to be defined incrementally as the project progresses:

* Removed entirely: `gpio_keys` (pushbutton), `pwmleds`, `can1`/`can2`, `adc1`, `die_temp`/`vref`/`vbat`, `dma1`, `plli2s`/`i2s3`, `i2c1` and its `cs43l22` audio codec node, `usbotg_fs`, and the `timers2`/`timers4` PWM configuration that only existed to drive the removed PWM LEDs.
* Reduced the four board LEDs to one (`green_led`, on `gpiod` pin 12), aliased as `led0` — enough to prove GPIO output works, with the remaining three left to be added back only if a future use actually needs them.
* Kept unchanged: the SoC-level includes (`stm32f407Xg.dtsi`, the pinctrl dtsi), the core clock tree (`clk_hse`, `pll`, `rcc` — the board's actual 8 MHz crystal driving a 168 MHz system clock, not reference-board-specific), and the console UART (`usart2`) — none of these are "extra" peripherals; they are what the board needs to boot and be observable at all.

Sensor buses (I²C/SPI) and any other peripherals the sensor node actually uses will be added back to the devicetree one at a time as each is wired up and integrated, rather than inheriting the reference board's full set upfront.

### Step 7 — First Build

The application skeleton (`CMakeLists.txt`, `prj.conf`, `src/main.c`) was filled in with a minimal GPIO blink of `led0`, and built:

```bash
cd stm32-sensor-node
west build -b stm32f4_sensor_node .
```

This first attempt failed twice before succeeding, for reasons worth recording:

1. `CMakeLists.txt` did not add itself to `BOARD_ROOT` (see [Section 2](#2-zephyr-board-directory-convention)) — the board was not found at all until that line was added.
2. Zephyr's own Python dependencies (`zephyr/scripts/requirements.txt`, notably `jsonschema`) had not actually been installed into the Python environment `west` was using on this machine, despite the workspace-level getting-started documentation assuming they were — `pip install -r zephyr/scripts/requirements.txt` had to be run into that specific interpreter first.

With both fixed, the build succeeded: 17.4 KB flash / 4.5 KB RAM used, `zephyr.elf` generated for `stm32f4_sensor_node`.

---

## 4. Worked Example — `sensor_node/stm32f4_sensor_node`

Applying the procedure above to this product's current target:

| Item | Reference (`st/stm32f4_disco`) | This board (`sensor_node/stm32f4_sensor_node`) |
|---|---|---|
| Directory | `zephyr/boards/st/stm32f4_disco/` | `stm32-sensor-node/boards/sensor_node/stm32f4_sensor_node/` |
| `board.yml` name / vendor | `stm32f4_disco` / `st` | `stm32f4_sensor_node` / `sensor_node` |
| `<name>.yaml` identifier / vendor | `stm32f4_disco` / `st` | `stm32f4_sensor_node` / `sensor_node` |
| Kconfig symbol | `BOARD_STM32F4_DISCO` | `BOARD_STM32F4_SENSOR_NODE` |
| `.dts` model / compatible | `"STMicroelectronics STM32F4DISCOVERY board"` / `"st,stm32f4discovery"` | `"STM32F4 Sensor Node"` / `"sensor_node,stm32f4-sensor-node"` |
| SoC | `stm32f407xx` (unchanged) | `stm32f407xx` (unchanged) |

**Current status:** All seven steps are complete. The board resolves under its own identity, the devicetree has been stripped to a minimal baseline (clocks, console, one LED), a build succeeds and produces a flashable `zephyr.elf`, and the firmware has been flashed onto the physical board and visually confirmed — the LED on `gpiod` pin 12 blinks at the expected ~1 Hz rate, so the pin mapping in the devicetree is correct, not just internally consistent.

**Remaining work, not yet done:**
* Add sensor bus peripherals (I²C/SPI) and any other hardware actually wired to the board, one at a time, as the project progresses — deliberately deferred rather than inherited from the reference board.
* `doc/index.rst` (carried over in Step 3) still documents the stock Discovery board's features and is not required for building — update or remove it when the board's own documentation is written.

---

## 5. Building and Flashing

### Prerequisites

* `west` and the Zephyr SDK, per the workspace-level getting-started documentation.
* Zephyr's Python dependencies installed into the interpreter `west` actually uses: `pip install -r zephyr/scripts/requirements.txt` (see Step 7's note above — this is easy to assume is already done and not actually be true).
* `openocd` (`sudo apt install openocd`) — the runner that talks to this board's onboard ST-Link/V2. Neither STM32CubeProgrammer nor a J-Link probe is used here.
* The board connected via its **ST-Link mini-USB port** (labeled `CN5`, next to the ST-Link status LEDs) — not the micro-USB OTG port on the opposite side, which is the target MCU's own USB peripheral, not the debug interface. Connected correctly, it enumerates over USB as STMicroelectronics `ST-LINK/V2.1` (USB vendor ID `0483`); `lsusb` can confirm this.

### Build

```bash
cd stm32-sensor-node
west build -b stm32f4_sensor_node . -d build
```

`-d build` places the build output inside the repository, at `stm32-sensor-node/build/` — matching the `.gitignore` entry that keeps it out of version control (see [Section 6](#6-gitignore)). Without `-d`, `west build` still defaults to a `build/` directory in the current working directory, so this is mostly about being explicit; it matters more once building from a different working directory becomes routine.

A clean build reports something like:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:       17488 B         1 MB      1.67%
             RAM:        4544 B       128 KB      3.47%
```

### Flash

```bash
west flash -d build --runner openocd
```

`--runner openocd` is passed explicitly because `board.cmake` lists `stm32cubeprogrammer` first as the default runner, and that tool is not installed on this machine — omitting `--runner` would attempt `stm32cubeprogrammer` first and fail before falling back.

A successful flash reports the detected chip (`stm32f4x.cpu`, an STM32F407VG, confirmed by chip ID `0x101f6413`), erases, and writes the firmware — after which the LED should visibly blink.

### Rebuilding after a code or devicetree change

```bash
west build -d build
```

(No `-b` or app path needed on subsequent builds — `west` remembers the board and source directory from the existing `build/` directory. `-p` / `--pristine` forces a clean rebuild if something seems stale.)

---

## 6. `.gitignore`

A `.gitignore` was added at the repository root once `build/` started appearing as untracked content after the first build — generated build output has no place in version control (see the workspace-level `getting_started.md`, Rule 2). It excludes:

```
build/, build-*/          — Zephyr/CMake build output
*.hex, *.bin, *.elf, *.map, *.o, *.a, *.d   — compiled/linked firmware artifacts
*.pyc, __pycache__/, .cache/                — Python bytecode and tool caches
.vscode/, *.swp, *.swo, *~, .DS_Store       — editor and OS cruft
```

---

## 7. References

* Zephyr Application Development: https://docs.zephyrproject.org/latest/develop/application/index.html
* Zephyr Board Porting Guide: https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html
* Zephyr Devicetree Guide: https://docs.zephyrproject.org/latest/build/dts/index.html
* Zephyr Pin Control: https://docs.zephyrproject.org/latest/hardware/pinctrl/index.html
* Reference board (`st/stm32f4_disco`) documentation: https://docs.zephyrproject.org/latest/boards/st/stm32f4_disco/doc/index.html
