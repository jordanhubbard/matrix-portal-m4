#!/bin/sh

# Example: ./run-demo.sh -D 0
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$SCRIPT_DIR/wrapper.sh" "$SCRIPT_DIR/rpi-rgb-led-matrix/examples-api-use/demo" "$@"
