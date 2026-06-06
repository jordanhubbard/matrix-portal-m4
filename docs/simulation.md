# Local simulation and preview

The useful split for this repository is:

- Repository validation: `make test`
- Real cross-build verification only: `make arduino-test`
- Local simulator IDE and example picker: `make sim-run`
- Direct visual preview: `make sim-run SKETCH=Arduino/rainbow/rainbow.ino`
- Hardware deployment: `make arduino-upload SKETCH=... PORT=/dev/cu...`

## Simulator survey

Wokwi is the best-known Arduino-focused simulator and supports many boards and
LED parts, but its documented hardware list does not include the SAMD51 Matrix
Portal M4 or HUB75/Protomatter output. It is also primarily a hosted/web
workflow, not a local macOS SDL app.

SimulIDE is a local desktop electronics simulator that can run compiled Arduino
firmware for supported MCU families. Its focus is circuit simulation for common
Arduino/PIC/AVR/ESP32-style targets, not a Matrix Portal M4 SAMD51 plus
Protomatter/HUB75 display model.

Renode is the strongest general-purpose option for serious Cortex-M firmware
emulation. It can run unmodified embedded binaries and model CPUs, peripherals,
sensors, and buses. The tradeoff is that Matrix Portal M4 support would require
custom SAMD51 and display peripheral modeling before it could show Protomatter
output.

simavr is useful for AVR Arduino firmware and can be paired with SDL frontends,
but Matrix Portal M4 is a SAMD51 Cortex-M4 target, so simavr is not the right
base for this board.

## Practical path in this repo

The `sim/` directory is a host-side SDL2 preview harness. It compiles Arduino
sketches as native macOS C++ and shadows the hardware-specific headers with
small compatibility stubs:

- `Adafruit_Protomatter` stores a 64x64 RGB565 framebuffer and renders it with
  SDL2 on `matrix.show()`.
- `Arduino.h` provides timing, random, GPIO, `digitalRead`, `digitalWrite`,
  `analogRead`, and `Serial` logging.
- `Adafruit_LIS3DH` reads from the emulator's virtual accelerometer state.
- `Adafruit_PixelDust` provides a lightweight particle approximation.
- `Adafruit_NeoPixel` mirrors pixel 0 as the Matrix Portal status NeoPixel.
- `WiFiNINA` provides a minimal connected-status stub for simple AirLift checks.

This does not replace hardware testing. It gives fast local feedback for display
logic and animation changes before cross-compiling and uploading.

## Matrix Portal hardware model

The emulator models the built-in hardware surfaces from the Matrix Portal M4
guide that are useful for local sketch development:

- HUB75 matrix output through editable virtual panels.
- LIS3DH accelerometer on I2C address `0x19`.
- UP/DOWN buttons on Arduino pins `2` and `3`, active low.
- RGB status NeoPixel on Arduino pin `4`.
- D13 status LED on Arduino pin `13`.
- A0 JST input and A1-A4 analog/GPIO pins through `analogRead`.
- ESP32 AirLift presence/status through a minimal `WiFiNINA` stub.

The emulator does not emulate SAMD51 instruction timing, QSPI flash contents,
real TLS/network traffic, USB storage, or external STEMMA QT devices.

## Simulator IDE

The preview window treats the sketch framebuffer as the logical output coming
from Protomatter and then lets you arrange physical panels on top of it. It
also provides a small SDL-native IDE shell around the running sketch.

Toolbar buttons and keys:

- Hover over any icon button for an English tooltip.
- The Stop button closes the current simulator or IDE window.
- In the IDE launcher, select a sketch, then use Run, Verify, Upload, Setup,
  Ports, Monitor, Open Sketch, or Open Log from the toolbar.
- `Enter` runs the selected sketch. `V`, `U`, `M`, and `O` trigger verify,
  upload, serial monitor, and open-sketch actions.
- The examples button, or `E`, opens the sketch picker. Choosing an example
  rebuilds and relaunches the simulator with that sketch.
- The verify button, or `V`, runs `make arduino-compile SKETCH=current`.
- The upload button, or `U`, opens a serial-port picker and then runs
  `make arduino-upload SKETCH=current PORT=selected`.
- The open-sketch button, or `O`, opens the current `.ino` file with macOS
  `open`.
- Drag a panel to move it.
- `A` adds a virtual panel.
- `Delete` removes the selected panel.
- `R` rotates the selected panel by 90 degrees.
- `F` fits panels into a row.
- `P` toggles the default new-panel size between 64x64 and 32x32.
- `S` saves the current arrangement to `sim-panel-layout.txt`.
- `L` reloads that arrangement.
- `G` toggles the grid.
- `+` and `-` zoom.

Board controls:

- Arrow keys adjust virtual accelerometer X/Y and disable auto-tilt.
- `0` returns the accelerometer to automatic synthetic tilt.
- `1` holds the UP button while pressed.
- `2` holds the DOWN button while pressed.
- `[` and `]` adjust A0.

The top strip also visualizes D13, the status NeoPixel, UP/DOWN button state,
accelerometer vector, and A0-A4 analog levels.

## Commands

Open the simulator IDE and SDL-native example file browser:

```sh
make sim-run
```

Build every preview app:

```sh
make sim-build-all
```

Run one preview:

```sh
make sim-run SKETCH=Arduino/stars/stars.ino
```

Run the Matrix Portal built-in device demo:

```sh
make sim-run SKETCH=Arduino/portal_controls/portal_controls.ino
```

Run for a fixed number of rendered frames:

```sh
make sim-run SKETCH=Arduino/life/life.ino SIM_MAX_FRAMES=300
```

If SDL2 is missing on macOS:

```sh
brew install sdl2
```
