# Arduino build notes

This repository uses Arduino CLI for reproducible Matrix Portal M4 cross-builds.
All Arduino CLI package data and sketchbook libraries are kept under the local
`.arduino/` directory through `arduino-cli.yaml`.

## Targets

- `make install` installs the repo-local Arduino CLI under `.tools/bin`, then
  installs the Adafruit SAMD core and pinned libraries under `.arduino/`.
- `make build` compiles every `Arduino/*/*.ino` sketch.
- `make build SKETCH=Arduino/rainbow/rainbow.ino` compiles one sketch.
- `make upload SKETCH=Arduino/rainbow/rainbow.ino PORT=/dev/cu...` compiles
  and uploads a sketch to a connected Matrix Portal M4.
- `make ports` lists connected boards and serial ports.
- `make clean` removes generated build outputs.
- `make run` starts the local SDL IDE/simulator. It does not upload to hardware.
- `make test` runs host checks, Arduino cross-builds, and SDL preview builds.

The Makefile discovers every `Arduino/*/*.ino` sketch, including new local demo
directories, so `make build` validates all code intended for upload.
Dependency installation targets are intentionally separate from test targets, so
routine validation does not mutate the local Arduino package state.

Sketches choose their own logical matrix width and height. Square demos mostly
use 64x64, while sign-oriented demos use the selected panel chain geometry to
set a wider framebuffer.

The sign-oriented examples share `Arduino/sign_common/SignDisplay.h` for the
Matrix Portal pin map, 5x7 text drawing, colors, and panel-chain setup. The
Makefile adds that directory to both Arduino CLI and SDL preview builds.

Display geometry can be selected in the SDL IDE before loading, verifying, or
uploading. Scripted builds use the same inputs:

```sh
make build SKETCH=Arduino/weather_dashboard/weather_dashboard.ino PANEL_COUNT=2 PANEL_WIDTH=64 PANEL_HEIGHT=32
make upload SKETCH=Arduino/weather_dashboard/weather_dashboard.ino PORT=/dev/cu.usbmodem... PANEL_COUNT=4 PANEL_WIDTH=32 PANEL_HEIGHT=32
```

The Makefile passes those values as `PANEL_COUNT`, `PANEL_WIDTH`,
`PANEL_HEIGHT`, `SIGN_PANEL_COUNT`, `SIGN_PANEL_WIDTH`, and
`SIGN_PANEL_HEIGHT` compiler defines. The simulator's `SIM_PANEL` setting is
derived from `PANEL_WIDTH` and `PANEL_HEIGHT`.

This repository does not install the official Arduino IDE application. Compile
and upload use `arduino-cli`; the local IDE mentioned in the README is the SDL
simulator shell built by `make run`.

## Dependency sources

The default library manifest is based on the current upstream
`library.properties` metadata for:

- Adafruit Protomatter: https://github.com/adafruit/Adafruit_Protomatter
- Adafruit GFX: https://github.com/adafruit/Adafruit-GFX-Library
- Adafruit LIS3DH: https://github.com/adafruit/Adafruit_LIS3DH
- Adafruit PixelDust: https://github.com/adafruit/Adafruit_PixelDust
- Adafruit BusIO: https://github.com/adafruit/Adafruit_BusIO
- Adafruit Unified Sensor: https://github.com/adafruit/Adafruit_Sensor
- Adafruit SPIFlash: https://github.com/adafruit/Adafruit_SPIFlash
- Adafruit TinyUSB Library: https://github.com/adafruit/Adafruit_TinyUSB_Arduino
- Adafruit NeoPixel: https://github.com/adafruit/Adafruit_NeoPixel
- SdFat - Adafruit Fork: https://github.com/adafruit/SdFat
- AnimatedGIF: https://github.com/bitbank2/AnimatedGIF
- MIDI Library: https://github.com/FortySevenEffects/arduino_midi_library

The optional MatrixPortal guide manifest also includes:

- WiFiNINA Adafruit fork: https://github.com/adafruit/WiFiNINA
- ArduinoJson: https://github.com/bblanchon/ArduinoJson
- Adafruit ImageReader: https://github.com/adafruit/Adafruit_ImageReader

Install those optional guide libraries with `make install-matrixportal-libs`
only when a project needs them.

The board target is from the Adafruit SAMD core:

- https://github.com/adafruit/ArduinoCore-samd
- https://adafruit.github.io/arduino-board-index/package_adafruit_index.json

## Updating dependency pins

Refresh the versions by reading each upstream `library.properties`, then update
`arduino-libraries.txt` and `arduino-matrixportal-libraries.txt`.

After updating, run:

```sh
make install
make test
```
