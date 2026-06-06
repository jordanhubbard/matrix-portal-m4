# Arduino build notes

This repository uses Arduino CLI for reproducible Matrix Portal M4 cross-builds.
All Arduino CLI package data and sketchbook libraries are kept under the local
`.arduino/` directory through `arduino-cli.yaml`.

## Targets

- `make arduino-install-cli ARDUINO_CLI=.tools/bin/arduino-cli` installs the
  Arduino CLI under `.tools/bin` for a repo-local setup. After that, the
  Makefile uses the local binary automatically.
- `make arduino-install` installs the Adafruit SAMD core and the pinned
  libraries needed by the checked-in sketches.
- `make setup` is the conventional alias for `make arduino-install`.
- `make arduino-compile` compiles every `Arduino/*/*.ino` sketch.
- `make arduino-compile SKETCH=Arduino/rainbow/rainbow.ino` compiles one
  sketch.
- `make arduino-test` cross-compiles Arduino sketches without installing or
  updating dependencies.
- `make test` runs host checks, Arduino cross-builds, and SDL preview builds.
- `make ci` is an alias for `make test`.
- `make arduino-install-matrixportal-libs` installs optional MatrixPortal guide
  libraries for WiFi and image-reader projects.
- `make arduino-clean` removes generated Arduino build products.
- `make arduino-ports` lists connected boards and serial ports.
- `make arduino-upload SKETCH=Arduino/rainbow/rainbow.ino PORT=/dev/cu...`
  uploads a sketch to a connected Matrix Portal M4.

The Makefile discovers every `Arduino/*/*.ino` sketch, including new local demo
directories, so `make arduino-compile` validates all code intended for upload.
Dependency installation targets are intentionally separate from test targets, so
routine validation does not mutate the local Arduino package state.

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

The board target is from the Adafruit SAMD core:

- https://github.com/adafruit/ArduinoCore-samd
- https://adafruit.github.io/arduino-board-index/package_adafruit_index.json

## Updating dependency pins

Refresh the versions by reading each upstream `library.properties`, then update
`arduino-libraries.txt` and `arduino-matrixportal-libraries.txt`.

After updating, run:

```sh
make setup
make test
```
