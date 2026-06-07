# matrix-portal-m4

Arduino and local-preview projects for an Adafruit Matrix Portal M4 driving
HUB75 RGB LED matrices.

The main workflow is now Makefile-driven:

- `arduino-cli` compiles and uploads sketches for the Matrix Portal M4.
- A native macOS AppKit launcher previews sketches on the host before hardware
  upload, with an AppKit/CoreGraphics HUB75 preview window.
- Display geometry is selected in the macOS IDE or through Make variables,
  then passed into sketches as compile-time panel-count and panel-size defines.
- Build products, Arduino package data, and optional local tools stay inside
  ignored repo directories.

This repository does not install the official Arduino IDE app. The "IDE" here
is the native macOS app bundle built under `build/sim/` by `make run`; hardware
build and upload are handled by `arduino-cli`.

## Quick Start

Install the repo-local Arduino CLI, the Adafruit SAMD board core, and pinned
sketch libraries:

```sh
make install
```

Compile every Arduino sketch for the Matrix Portal M4:

```sh
make build
```

Open the native macOS launcher IDE:

```sh
make run
```

Upload one sketch to hardware:

```sh
make ports
make upload SKETCH=Arduino/life/life.ino PORT=/dev/cu.usbmodem...
```

Run the full non-upload validation suite:

```sh
make test
```

## Public Targets

The normal commands are intentionally small:

```text
make install   install repo-local Arduino CLI, board core, and libraries
make build     compile all Arduino sketches, or SKETCH=... for one
make upload    compile and upload SKETCH=... to PORT=...
make ports     list connected Arduino serial ports
make run       open the macOS IDE, or SKETCH=... for one preview
make test      run host checks, Arduino cross-builds, and native preview builds
make clean     remove generated build outputs
make distclean remove build outputs, .tools/, and .arduino/
make doctor    print active toolchain and board configuration
```

Examples:

```sh
make build SKETCH=Arduino/rainbow/rainbow.ino
make run SKETCH=Arduino/pacman_demo/pacman_demo.ino
make run SKETCH=Arduino/stars/stars.ino
make run SKETCH=Arduino/life/life.ino SIM_MAX_FRAMES=300
make run PANEL_COUNT=2 PANEL_WIDTH=32 PANEL_HEIGHT=32
make upload SKETCH=Arduino/weather_dashboard/weather_dashboard.ino PORT=/dev/cu.usbmodem... PANEL_COUNT=3 PANEL_WIDTH=32 PANEL_HEIGHT=32
```

## Prerequisites

For Arduino hardware builds, `make install` downloads `arduino-cli` into
`.tools/bin` and installs Arduino package data and libraries under `.arduino/`
using `arduino-cli.yaml`.

The native launcher and preview renderer are built with the Xcode command line
tools: `swiftc` for the IDE and Apple clang for the Objective-C++ preview.

Uploading requires a Matrix Portal M4 connected over USB. Use `make ports` to
find the serial device name.

## Arduino Sketches

Sketches live under `Arduino/<name>/<name>.ino`. The Makefile discovers this
layout automatically, so new sketches are included in `make build` and
`make test` without editing the Makefile.

Current sketches:

- `breakout_demo` - self-playing brick breaker.
- `clock_display` - sign clock and date-style display.
- `fire` - flame animation.
- `gif_player` - built-in animated image playback demo.
- `life` - Conway's Game of Life.
- `maze` - generated maze with animated traversal.
- `ocean_waves` - layered wave animation.
- `pacman_demo` - self-playing maze chase demo for 1-4 chained 32x32 panels.
- `pixeldust` - accelerometer-driven particle demo.
- `plasma` - procedural plasma pattern.
- `pong_demo` - self-playing Pong.
- `portal_controls` - Matrix Portal built-in controls and status demo.
- `rainbow` - rainbow matrix test pattern.
- `random_shapes` - animated shape drawing.
- `rotating_sand` - PixelDust-style rotating sand.
- `scrolling_text` - ticker-style text sign.
- `simple_m4` - basic Matrix Portal hardware test.
- `snake_demo` - self-playing Snake.
- `stars` - starfield animation.
- `tetris_demo` - falling-block attract-mode demo.
- `vu_meter` - synthetic audio VU/spectrum display.
- `weather_dashboard` - simulated weather dashboard.

The default board target is:

```text
adafruit:samd:adafruit_matrixportal_m4:cache=on,speed=120,opt=small,maxqspi=50,usbstack=arduino,debug=off
```

## Native macOS IDE

The `sim/macos/` directory builds a native AppKit app bundle. The `sim/`
compatibility layer compiles sketches as host C++/Objective-C++ and shadows
selected Arduino, Protomatter, LIS3DH, PixelDust, NeoPixel, and WiFiNINA APIs
with local stubs.

The simulator is useful for display logic and interaction checks before upload.
It is not a cycle-accurate SAMD51 emulator and does not replace hardware
testing.

With no `SKETCH`, `make run` opens the simulator IDE. The right-side
configuration panel has an explicit destination mode:

- `Simulator` loads the selected sketch into the native preview with emulated
  serial I/O, buttons, analog inputs, and sensor values.
- `Hardware` loads the selected sketch to the selected USB device. The USB
  device and baud rate are selected in first-class dropdowns, and the I/O drawer
  switches from emulated serial to a live USB serial text pane.

The serial I/O drawer is a terminal-style text window with scrollback and a TX
input line. In Simulator mode, build output and sketch `Serial.print()` output
stream into the text pane, and TX is written to the running simulator process.
In Hardware mode, selecting a USB device opens that serial port at the selected
baud rate, incoming bytes are shown in the RX scrollback, and Enter sends the
TX line to the board.

The same panel has first-class display geometry controls. Choose the number of
chained panels and each panel's pixel size before loading, verifying, or
uploading. The IDE passes those choices as `PANEL_COUNT`, `PANEL_WIDTH`,
`PANEL_HEIGHT`, `SIGN_PANEL_COUNT`, `SIGN_PANEL_WIDTH`, and
`SIGN_PANEL_HEIGHT` build defines, so sign sketches compile for the selected
matrix width instead of relying on a hardcoded `SIM_PANEL` default.

When Hardware mode is selected, sensor simulation controls are shown as disabled
because the physical Matrix Portal sensors and inputs are in the loop.

The simulated Matrix Portal surface includes:

- HUB75 framebuffer output with editable panel layout and selectable 32x16,
  32x32, 64x32, or 64x64 module geometry.
- LIS3DH accelerometer input.
- UP/DOWN buttons on pins `2` and `3`.
- D13 LED and RGB status NeoPixel.
- A0-A4 analog inputs.
- Minimal WiFiNINA connected-status stub.

## Repository Layout

- `Arduino/` - Matrix Portal M4 Arduino sketches.
- `Arduino/sign_common/` - shared sign-demo font, color, and matrix helpers.
- `sim/macos/` - native AppKit launcher IDE.
- `sim/` - native preview renderer and compatibility stubs.
- `docs/arduino.md` - Arduino CLI dependency and pin notes.
- `docs/simulation.md` - simulator behavior and controls.
- `CircuitPython/Life/` - older CircuitPython Life port kept for reference.
- `RaspberryPI/` - helper scripts for the `rpi-rgb-led-matrix` submodule.
- `.arduino/` - repo-local Arduino package data, ignored by Git.
- `.tools/` - optional repo-local tool binaries, ignored by Git.

## Hardware Notes

The sign-oriented demos, such as `pacman_demo`, are built for the selected
panel chain geometry. The default is four 32x32 HUB75 modules, but the IDE and
Make variables can build the same sketches for 1-4 panels and 16-, 32-, or
64-pixel-high modules. Older square demos mostly keep their own logical 64x64
framebuffer and ignore the sign geometry defines.

All sketches use the Matrix Portal M4 pin mapping from Adafruit Protomatter
examples:

```cpp
uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;
```

For 64x64 panels, make sure the panel address E line is connected as required
by the Matrix Portal M4 hardware.

## Maintenance

Pinned Arduino libraries live in `arduino-libraries.txt`. Optional libraries
from Adafruit's MatrixPortal M4 guide live in
`arduino-matrixportal-libraries.txt`; install them with
`make install-matrixportal-libs` only when a project needs WiFi or image-reader
support.

After changing dependency pins or sketch code, run:

```sh
make install
make test
```
