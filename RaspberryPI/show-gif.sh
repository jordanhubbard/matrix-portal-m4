#!/bin/sh
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$SCRIPT_DIR/wrapper.sh" "$SCRIPT_DIR/rpi-rgb-led-matrix/utils/led-image-viewer" "$@" -C --led-limit-refresh=120
