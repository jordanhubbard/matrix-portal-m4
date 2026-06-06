# matrix-portal-m4

Small Matrix Portal M4 projects for HUB75 RGB LED matrices.

The primary target is an Adafruit Matrix Portal M4, which is a SAMD51 board
with MatrixPortal pin mapping and an onboard LIS3DH accelerometer. Most Arduino
sketches in this repository are configured for one 64x64 panel.

## Contents

- `Arduino/` - Arduino sketches using Adafruit Protomatter.
- `CircuitPython/Life/` - a CircuitPython Conway's Game of Life port for two
  64x64 panels.
- `RaspberryPI/` - helper wrappers for the `rpi-rgb-led-matrix` submodule.

Current Arduino demos include fire, Life, maze, ocean waves, PixelDust,
plasma, rainbow, random shapes, a Matrix Portal controls demo, rotating sand,
simple hardware test, and starfield.

## Arduino quick start

Install `arduino-cli`, then let the Makefile install the Adafruit SAMD core and
the pinned libraries used by the checked-in sketches:

```sh
make arduino-install
make test
```

For a repo-local CLI install instead of a global install:

```sh
make arduino-install-cli ARDUINO_CLI=.tools/bin/arduino-cli
make setup
make test
```

To build a single sketch:

```sh
make arduino-compile SKETCH=Arduino/life/life.ino
```

To upload a sketch over serial:

```sh
make arduino-ports
make arduino-upload SKETCH=Arduino/life/life.ino PORT=/dev/cu.usbmodem...
```

The default cross-build target is:

```text
adafruit:samd:adafruit_matrixportal_m4:cache=on,speed=120,opt=small,maxqspi=50,usbstack=arduino,debug=off
```

Use `make doctor` to print the active target, dependency files, and discovered
sketches. Use `make setup` for dependency installation and `make test` for the
non-upload validation suite: host checks, Arduino cross-builds, and SDL preview
builds. `make arduino-test` only cross-compiles Arduino sketches; it does not
install or update dependencies.

## Local SDL preview

The `sim/` harness builds a local macOS SDL2 application for quick visual
previews of Arduino sketches before uploading to hardware. With no `SKETCH`
argument it opens a small all-in-one SDL IDE for choosing examples, installing
dependencies, running the simulator, verifying, uploading over serial, opening
the command log, starting a serial monitor, and arranging virtual LED panels:

```sh
make sim-build-all
make sim-run
```

This is a host preview layer, not a cycle-accurate SAMD51 hardware emulator.
It replaces Arduino, Protomatter, LIS3DH, and PixelDust APIs with local stubs
and renders the matrix framebuffer through SDL2. The preview also simulates
Matrix Portal built-ins including the LIS3DH accelerometer, UP/DOWN buttons,
D13 LED, status NeoPixel, A0-A4 analog reads, and a minimal WiFiNINA status
stub. Hover over any icon button in the simulator for an English tooltip.
Use `make sim-run SKETCH=Arduino/rainbow/rainbow.ino` when you want to bypass
the IDE picker and run one sketch directly.

## Hardware notes

For 64x64 panels, make sure the panel's address E line is connected as required
by the Matrix Portal M4 hardware. The sketches use the MatrixPortal M4 pin
mapping from Adafruit's Protomatter examples:

```cpp
uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20, 21};
uint8_t clockPin   = 14;
uint8_t latchPin   = 15;
uint8_t oePin      = 16;
```

## Maintenance

Pinned Arduino libraries live in `arduino-libraries.txt`. Optional libraries
from Adafruit's MatrixPortal M4 Arduino guide live in
`arduino-matrixportal-libraries.txt`.

The Arduino CLI configuration is repo-local in `arduino-cli.yaml`; package and
library installs go under `.arduino/`, and optional local tools go under
`.tools/`. Both directories are intentionally ignored by Git.
