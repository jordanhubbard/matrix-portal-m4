# Local simulation and preview

The useful split for this repository is:

- Repository validation: `make test`
- Real cross-build verification only: `make build`
- Native macOS simulator IDE and example picker: `make run`
- Direct visual preview: `make run SKETCH=Arduino/rainbow/rainbow.ino`
- 32x32 sign preview: `make run SKETCH=Arduino/pacman_demo/pacman_demo.ino PANEL_COUNT=4 PANEL_WIDTH=32 PANEL_HEIGHT=32`
- Hardware deployment: `make upload SKETCH=... PORT=/dev/cu...`

## Simulator survey

Wokwi is the best-known Arduino-focused simulator and supports many boards and
LED parts, but its documented hardware list does not include the SAMD51 Matrix
Portal M4 or HUB75/Protomatter output. It is also primarily a hosted/web
workflow, not a local macOS app.

SimulIDE is a local desktop electronics simulator that can run compiled Arduino
firmware for supported MCU families. Its focus is circuit simulation for common
Arduino/PIC/AVR/ESP32-style targets, not a Matrix Portal M4 SAMD51 plus
Protomatter/HUB75 display model.

Renode is the strongest general-purpose option for serious Cortex-M firmware
emulation. It can run unmodified embedded binaries and model CPUs, peripherals,
sensors, and buses. The tradeoff is that Matrix Portal M4 support would require
custom SAMD51 and display peripheral modeling before it could show Protomatter
output.

simavr is useful for AVR Arduino firmware, but Matrix Portal M4 is a SAMD51
Cortex-M4 target, so simavr is not the right base for this board.

## Practical path in this repo

The `sim/` directory is a host-side native preview harness. It compiles Arduino
sketches as native macOS C++/Objective-C++ and shadows the hardware-specific
headers with small compatibility stubs:

- `Adafruit_Protomatter` stores a 64x64 RGB565 framebuffer and renders it with
  CoreGraphics in an AppKit preview window on `matrix.show()`.
- `Arduino.h` provides timing, random, GPIO, `digitalRead`, `digitalWrite`,
  `analogRead`, and `Serial` logging.
- `Adafruit_LIS3DH` reads from the emulator's virtual accelerometer state.
- `Adafruit_PixelDust` provides a lightweight particle approximation.
- `Adafruit_NeoPixel` mirrors pixel 0 as the Matrix Portal status NeoPixel.
- `WiFiNINA` provides a minimal connected-status stub for simple AirLift checks.

This does not replace hardware testing. It gives fast local feedback for display
logic and animation changes before cross-compiling and uploading. The default
virtual panel size is 32x32 because many sign builds chain two to four 32x32
modules in a row.

## Matrix Portal hardware model

The emulator models the built-in hardware surfaces from the Matrix Portal M4
guide that are useful for local sketch development:

- HUB75 matrix output through editable 32x32 or 64x64 virtual panels.
- LIS3DH accelerometer on I2C address `0x19`.
- UP/DOWN buttons on Arduino pins `2` and `3`, active low.
- RGB status NeoPixel on Arduino pin `4`.
- D13 status LED on Arduino pin `13`.
- A0 JST input and A1-A4 analog/GPIO pins through `analogRead`.
- ESP32 AirLift presence/status through a minimal `WiFiNINA` stub.

The emulator does not emulate SAMD51 instruction timing, QSPI flash contents,
real TLS/network traffic, USB storage, or external STEMMA QT devices.

## Native macOS IDE

With no `SKETCH`, `make run` builds and launches the native Swift/AppKit IDE.
It uses macOS controls for sketch selection, commands, destination mode, display
geometry, USB port selection, baud selection, serial text I/O, and simulated
device controls. Direct sketch previews use an AppKit/CoreGraphics window for
the HUB75 framebuffer.

The IDE has two explicit destination modes:

- `Simulator` compiles and launches the selected sketch in the native preview.
  Build output and sketch serial output stream into the serial text pane, and
  Matrix Portal sensors, buttons, and analog inputs are simulated.
- `Hardware` compiles and uploads the selected sketch to the selected USB
  device. The USB device and baud rate are selected from dropdowns in the
  configuration panel. In this mode, the serial drawer opens the physical USB
  serial port and sensor simulation controls are disabled because the real board
  is providing those inputs.

The serial drawer is an in-app text window, not just connection metadata. It has
RX scrollback and a TX input line. Simulator mode uses an emulated loopback
serial endpoint; Hardware mode opens the selected `/dev/cu...` device at the
selected baud rate, displays incoming bytes, and writes the TX line when Enter
is pressed.

The IDE also has display geometry controls for the code that will be built
next. Select 1-4 chained panels and a panel size of 32x16, 32x32, 64x32, or
64x64. Load, Verify, and Upload pass those choices through Make as
`PANEL_COUNT`, `PANEL_WIDTH`, and `PANEL_HEIGHT`; the Makefile mirrors them to
`SIGN_PANEL_COUNT`, `SIGN_PANEL_WIDTH`, and `SIGN_PANEL_HEIGHT` for shared sign
examples. `SIM_PANEL` is derived from the selected panel size, so it only
describes the simulator's physical module dimensions.

IDE controls:

- Select a sketch in the native table and use Load, Verify, Upload, Stop,
  Ports, or Open from the macOS toolbar or menus.
- Load in Simulator mode launches `make run SKETCH=current` with the selected
  geometry.
- Load or Upload in Hardware mode runs `make upload SKETCH=current
  PORT=selected` and reconnects the serial text pane after upload finishes.
- The native serial pane shows RX scrollback and sends the TX line when Return
  is pressed.

Preview controls:

- The direct preview window renders the matrix framebuffer and supports panel
  layout editing from its toolbar:
- Drag a panel to move it.
- Add or delete a virtual panel.
- Rotate the selected panel by 90 degrees.
- Fit panels into a row.
- Save or load `sim-panel-layout.txt`.
- Toggle the grid and zoom the preview.

Board controls:

- Arrow keys adjust virtual accelerometer X/Y and disable auto-tilt.
- `0` returns the accelerometer to automatic synthetic tilt.
- `1` holds the UP button while pressed.
- `2` holds the DOWN button while pressed.
- `[` and `]` adjust A0.

## Commands

Open the native macOS IDE and example browser:

```sh
make run
```

Build every preview app:

```sh
make test
```

Run one preview:

```sh
make run SKETCH=Arduino/stars/stars.ino
```

Run the self-playing Pac-Man-style sign demo:

```sh
make run SKETCH=Arduino/pacman_demo/pacman_demo.ino
```

Run the same sign demo as a two-panel 64x32 sign:

```sh
make run SKETCH=Arduino/pacman_demo/pacman_demo.ino PANEL_COUNT=2 PANEL_WIDTH=64 PANEL_HEIGHT=32
```

Run the Matrix Portal built-in device demo:

```sh
make run SKETCH=Arduino/portal_controls/portal_controls.ino
```

Run for a fixed number of rendered frames:

```sh
make run SKETCH=Arduino/life/life.ino SIM_MAX_FRAMES=300
```
