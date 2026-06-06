SHELL := /bin/sh
.SUFFIXES:
.DELETE_ON_ERROR:

.DEFAULT_GOAL := help
.NOTPARALLEL: install test run

TOOLS_DIR ?= .tools
BUILD_DIR ?= build

ARDUINO_CLI_VERSION ?= latest
LOCAL_ARDUINO_CLI := $(TOOLS_DIR)/bin/arduino-cli
ARDUINO_CLI ?= $(if $(wildcard $(LOCAL_ARDUINO_CLI)),$(LOCAL_ARDUINO_CLI),arduino-cli)
ARDUINO_CONFIG ?= arduino-cli.yaml
ADAFRUIT_SAMD_VERSION ?= 1.7.17

ARDUINO_BOARD ?= adafruit:samd:adafruit_matrixportal_m4
ARDUINO_BOARD_OPTIONS ?= cache=on,speed=120,opt=small,maxqspi=50,usbstack=arduino,debug=off
ARDUINO_FQBN ?= $(ARDUINO_BOARD):$(ARDUINO_BOARD_OPTIONS)
ARDUINO_LIBRARIES_FILE ?= arduino-libraries.txt
ARDUINO_MATRIXPORTAL_LIBRARIES_FILE ?= arduino-matrixportal-libraries.txt
ARDUINO_BUILD_DIR ?= $(BUILD_DIR)/arduino
ARDUINO_SKETCHES ?= $(shell find Arduino -mindepth 2 -maxdepth 2 -name '*.ino' | LC_ALL=C sort)
PANEL_COUNT ?= 4
PANEL_WIDTH ?= 32
PANEL_HEIGHT ?= 32
PANEL_CPPFLAGS := -I$(abspath Arduino/sign_common) -DSIGN_PANEL_COUNT=$(PANEL_COUNT) -DSIGN_PANEL_WIDTH=$(PANEL_WIDTH) -DSIGN_PANEL_HEIGHT=$(PANEL_HEIGHT) -DPANEL_COUNT=$(PANEL_COUNT) -DPANEL_WIDTH=$(PANEL_WIDTH) -DPANEL_HEIGHT=$(PANEL_HEIGHT)

SIM_BUILD_DIR ?= $(BUILD_DIR)/sim
SIM_TARGET_SKETCH := $(strip $(SKETCH))
SIM_SKETCH_NAME := $(notdir $(basename $(SIM_TARGET_SKETCH)))
SIM_BIN := $(SIM_BUILD_DIR)/$(SIM_SKETCH_NAME)-sim
SIM_IDE_BIN := $(SIM_BUILD_DIR)/matrix-portal-ide
SIM_SCALE ?= 8
SIM_MAX_FRAMES ?= 0
SIM_PANEL ?= $(PANEL_WIDTH)x$(PANEL_HEIGHT)
SIM_LAYOUT ?= sim-panel-layout.txt
SIM_CXX ?= c++
SDL2_CONFIG ?= sdl2-config
SIM_CPPFLAGS ?= -Isim/include -IArduino/sign_common
SIM_CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wno-unused-parameter
SIM_SOURCES := sim/src/sketch_runner.cpp sim/src/Arduino.cpp sim/src/Adafruit_Protomatter.cpp
SIM_IDE_SOURCES := sim/src/sim_ide.cpp

SHELL_SCRIPTS := $(shell find RaspberryPI -maxdepth 1 -name '*.sh' | LC_ALL=C sort)

.PHONY: help doctor install build upload ports run test clean distclean submodules
.PHONY: install-matrixportal-libs
.PHONY: _arduino-cli _sim-sdl _sim-build _sim-build-all

help: ## Show available targets.
	@awk 'BEGIN {FS = ":.*##"; printf "Targets:\n"} /^[a-zA-Z0-9_.-]+:.*##/ {printf "  %-24s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

doctor: ## Print local tool and build configuration.
	@printf "Arduino CLI: "
	@if command -v "$(ARDUINO_CLI)" >/dev/null 2>&1; then "$(ARDUINO_CLI)" version; else echo "missing ($(ARDUINO_CLI))"; fi
	@printf "Arduino config: %s\n" "$(ARDUINO_CONFIG)"
	@printf "Arduino core:   adafruit:samd@%s\n" "$(ADAFRUIT_SAMD_VERSION)"
	@printf "Arduino FQBN:   %s\n" "$(ARDUINO_FQBN)"
	@printf "Panel geometry: %s x %sx%s\n" "$(PANEL_COUNT)" "$(PANEL_WIDTH)" "$(PANEL_HEIGHT)"
	@printf "SDL2 config:    "
	@if command -v "$(SDL2_CONFIG)" >/dev/null 2>&1; then "$(SDL2_CONFIG)" --version; else echo "missing ($(SDL2_CONFIG))"; fi
	@printf "Sketches:\n"
	@for sketch in $(ARDUINO_SKETCHES); do printf "  %s\n" "$$sketch"; done

install: $(LOCAL_ARDUINO_CLI) ## Install repo-local Arduino CLI, board core, and sketch libraries.
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" core update-index
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" core install "adafruit:samd@$(ADAFRUIT_SAMD_VERSION)"
	@set -eu; \
	while IFS= read -r lib; do \
		case "$$lib" in ""|\#*) continue ;; esac; \
		printf 'arduino-cli lib install %s\n' "$$lib"; \
		"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" lib install "$$lib"; \
	done < "$(ARDUINO_LIBRARIES_FILE)"

build: _arduino-cli ## Compile Arduino sketches, or SKETCH=path for one sketch.
	@set -eu; \
	if [ -n "$(strip $(SKETCH))" ]; then \
		set -- "$(SKETCH)"; \
	else \
		set -- $(ARDUINO_SKETCHES); \
	fi; \
	if [ "$$#" -eq 0 ]; then \
		echo "error: no Arduino sketches found"; \
		exit 1; \
	fi; \
	for sketch do \
		case "$$sketch" in \
			*.ino) ;; \
			*) echo "error: SKETCH must point to an .ino file: $$sketch"; exit 2 ;; \
		esac; \
		test -f "$$sketch" || { echo "error: sketch not found: $$sketch"; exit 2; }; \
		name=$$(basename "$$(dirname "$$sketch")"); \
		build_path="$(ARDUINO_BUILD_DIR)/$$name"; \
		mkdir -p "$$build_path"; \
		printf '==> %s\n' "$$sketch"; \
		"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" compile \
			--fqbn "$(ARDUINO_FQBN)" \
			--build-property compiler.cpp.extra_flags="$(PANEL_CPPFLAGS)" \
			--build-path "$$build_path" \
			"$$sketch"; \
	done

upload: _arduino-cli ## Compile and upload SKETCH=path to PORT=/dev/cu.*.
	@set -eu; \
	test -n "$(strip $(SKETCH))" || { echo "error: set SKETCH=Arduino/name/name.ino"; exit 2; }; \
	test -n "$(strip $(PORT))" || { echo "error: set PORT=/dev/cu.usbmodem..."; exit 2; }; \
	test -f "$(SKETCH)" || { echo "error: sketch not found: $(SKETCH)"; exit 2; }; \
	name=$$(basename "$$(dirname "$(SKETCH)")"); \
	build_path="$(ARDUINO_BUILD_DIR)/$$name"; \
	mkdir -p "$$build_path"; \
	printf '==> compile %s\n' "$(SKETCH)"; \
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" compile \
		--fqbn "$(ARDUINO_FQBN)" \
		--build-property compiler.cpp.extra_flags="$(PANEL_CPPFLAGS)" \
		--build-path "$$build_path" \
		"$(SKETCH)"; \
	printf '==> upload %s to %s\n' "$(SKETCH)" "$(PORT)"; \
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" upload \
		--fqbn "$(ARDUINO_FQBN)" \
		--port "$(PORT)" \
		--input-dir "$$build_path" \
		"$(SKETCH)"

ports: _arduino-cli ## List connected Arduino serial ports.
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" board list

run: _sim-build ## Run SDL IDE with no SKETCH, or run one preview with SKETCH=path.
	@if [ -z "$(SIM_TARGET_SKETCH)" ]; then \
		"$(SIM_IDE_BIN)" --scale "$(SIM_SCALE)" --panel "$(SIM_PANEL)" --panel-count "$(PANEL_COUNT)" --layout "$(SIM_LAYOUT)"; \
	else \
		"$(SIM_BIN)" --scale "$(SIM_SCALE)" --max-frames "$(SIM_MAX_FRAMES)" --panel "$(SIM_PANEL)" --layout "$(SIM_LAYOUT)"; \
	fi

test: ## Run host checks, Arduino cross-builds, and SDL preview builds.
	@set -eu; \
	for script in $(SHELL_SCRIPTS); do \
		printf 'sh -n %s\n' "$$script"; \
		sh -n "$$script"; \
	done
	@python3 -m py_compile CircuitPython/Life/code.py
	@rm -rf CircuitPython/Life/__pycache__
	@$(MAKE) --no-print-directory build
	@$(MAKE) --no-print-directory _sim-build
	@$(MAKE) --no-print-directory _sim-build-all

clean: ## Remove generated build outputs.
	rm -rf "$(ARDUINO_BUILD_DIR)" "$(SIM_BUILD_DIR)"

distclean: clean ## Remove generated build outputs, local tools, and local Arduino package data.
	rm -rf "$(TOOLS_DIR)" .arduino

submodules: ## Initialize or update submodules used by Raspberry Pi helpers.
	git submodule update --init --recursive

$(LOCAL_ARDUINO_CLI):
	@mkdir -p "$(TOOLS_DIR)/bin"
	curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$(abspath $(TOOLS_DIR)/bin)" sh -s "$(ARDUINO_CLI_VERSION)"
	@"$@" version

install-matrixportal-libs: _arduino-cli
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" core update-index
	@set -eu; \
	while IFS= read -r lib; do \
		case "$$lib" in ""|\#*) continue ;; esac; \
		printf 'arduino-cli lib install %s\n' "$$lib"; \
		"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" lib install "$$lib"; \
	done < "$(ARDUINO_MATRIXPORTAL_LIBRARIES_FILE)"

_arduino-cli:
	@command -v "$(ARDUINO_CLI)" >/dev/null 2>&1 || { \
		echo "error: $(ARDUINO_CLI) is not installed or not on PATH"; \
		echo "run: make install"; \
		echo "manual install: https://arduino.github.io/arduino-cli/latest/installation/"; \
		exit 127; \
	}

_sim-sdl:
	@command -v "$(SDL2_CONFIG)" >/dev/null 2>&1 || { \
		echo "error: $(SDL2_CONFIG) is not installed or not on PATH"; \
		echo "on macOS, run: brew install sdl2"; \
		exit 127; \
	}

_sim-build: _sim-sdl
	@set -eu; \
	sketch="$(SIM_TARGET_SKETCH)"; \
	mkdir -p "$(SIM_BUILD_DIR)"; \
	if [ -z "$$sketch" ]; then \
		printf '==> SDL IDE\n'; \
		"$(SIM_CXX)" $(SIM_CPPFLAGS) $(PANEL_CPPFLAGS) $(SIM_CXXFLAGS) $$("$(SDL2_CONFIG)" --cflags) \
			$(SIM_IDE_SOURCES) $$("$(SDL2_CONFIG)" --libs) \
			-o "$(SIM_IDE_BIN)"; \
	else \
		test -f "$$sketch" || { echo "error: sketch not found: $$sketch"; exit 2; }; \
		name=$$(basename "$$(dirname "$$sketch")"); \
		bin="$(SIM_BUILD_DIR)/$$name-sim"; \
		printf '==> %s\n' "$$sketch"; \
		"$(SIM_CXX)" $(SIM_CPPFLAGS) $(PANEL_CPPFLAGS) $(SIM_CXXFLAGS) $$("$(SDL2_CONFIG)" --cflags) \
			-DSKETCH_SOURCE="\"$(abspath $(SIM_TARGET_SKETCH))\"" \
			-DSKETCH_NAME="\"$(SIM_TARGET_SKETCH)\"" \
			$(SIM_SOURCES) $$("$(SDL2_CONFIG)" --libs) \
			-o "$$bin"; \
	fi

_sim-build-all: _sim-sdl
	@set -eu; \
	if [ -z "$(ARDUINO_SKETCHES)" ]; then \
		echo "error: no Arduino sketches found"; \
		exit 1; \
	fi; \
	for sketch in $(ARDUINO_SKETCHES); do \
		name=$$(basename "$$(dirname "$$sketch")"); \
		bin="$(SIM_BUILD_DIR)/$$name-sim"; \
		mkdir -p "$(SIM_BUILD_DIR)"; \
		printf '==> %s\n' "$$sketch"; \
		"$(SIM_CXX)" $(SIM_CPPFLAGS) $(PANEL_CPPFLAGS) $(SIM_CXXFLAGS) $$("$(SDL2_CONFIG)" --cflags) \
			-DSKETCH_SOURCE="\"$(abspath .)/$$sketch\"" \
			-DSKETCH_NAME="\"$$sketch\"" \
			$(SIM_SOURCES) $$("$(SDL2_CONFIG)" --libs) \
			-o "$$bin"; \
	done
