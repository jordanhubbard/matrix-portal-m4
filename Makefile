SHELL := /bin/sh
.SUFFIXES:
.DELETE_ON_ERROR:

.DEFAULT_GOAL := help
.NOTPARALLEL: setup arduino-install arduino-install-core arduino-install-libs arduino-install-matrixportal-libs arduino-update-index

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

SIM_BUILD_DIR ?= $(BUILD_DIR)/sim
SIM_TARGET_SKETCH := $(strip $(SKETCH))
SIM_SKETCH_NAME := $(notdir $(basename $(SIM_TARGET_SKETCH)))
SIM_BIN := $(SIM_BUILD_DIR)/$(SIM_SKETCH_NAME)-sim
SIM_IDE_BIN := $(SIM_BUILD_DIR)/matrix-portal-ide
SIM_SCALE ?= 8
SIM_MAX_FRAMES ?= 0
SIM_PANEL ?= 64x64
SIM_LAYOUT ?= sim-panel-layout.txt
SIM_CXX ?= c++
SDL2_CONFIG ?= sdl2-config
SIM_CPPFLAGS ?= -Isim/include
SIM_CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wno-unused-parameter
SIM_SOURCES := sim/src/sketch_runner.cpp sim/src/Arduino.cpp sim/src/Adafruit_Protomatter.cpp
SIM_IDE_SOURCES := sim/src/sim_ide.cpp

SHELL_SCRIPTS := $(shell find RaspberryPI -maxdepth 1 -name '*.sh' | LC_ALL=C sort)

.PHONY: help doctor setup test ci check shell-check python-check
.PHONY: submodules
.PHONY: arduino-check-cli arduino-install-cli arduino-update-index
.PHONY: arduino-install-core arduino-install-libs arduino-install-matrixportal-libs
.PHONY: arduino-install arduino-test arduino-compile arduino-ports arduino-upload
.PHONY: sim-check-sdl sim-build sim-build-all sim-run sim-test
.PHONY: arduino-clean sim-clean clean distclean

help: ## Show available targets.
	@awk 'BEGIN {FS = ":.*##"; printf "Targets:\n"} /^[a-zA-Z0-9_.-]+:.*##/ {printf "  %-34s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

doctor: ## Print local tool and build configuration.
	@printf "Arduino CLI: "
	@if command -v "$(ARDUINO_CLI)" >/dev/null 2>&1; then "$(ARDUINO_CLI)" version; else echo "missing ($(ARDUINO_CLI))"; fi
	@printf "Arduino config: %s\n" "$(ARDUINO_CONFIG)"
	@printf "Arduino core:   adafruit:samd@%s\n" "$(ADAFRUIT_SAMD_VERSION)"
	@printf "Arduino FQBN:   %s\n" "$(ARDUINO_FQBN)"
	@printf "SDL2 config:    "
	@if command -v "$(SDL2_CONFIG)" >/dev/null 2>&1; then "$(SDL2_CONFIG)" --version; else echo "missing ($(SDL2_CONFIG))"; fi
	@printf "Sketches:\n"
	@for sketch in $(ARDUINO_SKETCHES); do printf "  %s\n" "$$sketch"; done

setup: arduino-install ## Install Arduino board core and sketch libraries.

test: check arduino-compile sim-test ## Run local checks, Arduino cross-builds, and SDL preview builds.

ci: test ## Alias for the full non-upload validation suite.

check: shell-check python-check ## Run host-side checks that do not need Arduino CLI.

shell-check: ## Syntax-check repository shell scripts.
	@set -eu; \
	for script in $(SHELL_SCRIPTS); do \
		printf 'sh -n %s\n' "$$script"; \
		sh -n "$$script"; \
	done

python-check: ## Syntax-check CircuitPython sources with CPython parser.
	@python3 -m py_compile CircuitPython/Life/code.py
	@rm -rf CircuitPython/Life/__pycache__

submodules: ## Initialize or update submodules used by Raspberry Pi helpers.
	git submodule update --init --recursive

arduino-check-cli: ## Verify arduino-cli is available.
	@command -v "$(ARDUINO_CLI)" >/dev/null 2>&1 || { \
		echo "error: $(ARDUINO_CLI) is not installed or not on PATH"; \
		echo "run: make arduino-install-cli ARDUINO_CLI=$(LOCAL_ARDUINO_CLI)"; \
		echo "install it from https://arduino.github.io/arduino-cli/latest/installation/"; \
		exit 127; \
	}

arduino-install-cli: ## Install Arduino CLI locally under .tools/bin.
	@mkdir -p "$(TOOLS_DIR)/bin"
	curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$(abspath $(TOOLS_DIR)/bin)" sh -s "$(ARDUINO_CLI_VERSION)"
	@"$(LOCAL_ARDUINO_CLI)" version

arduino-update-index: arduino-check-cli ## Refresh Arduino and Adafruit package indexes.
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" core update-index

arduino-install-core: arduino-update-index ## Install the Adafruit SAMD core for Matrix Portal M4 cross-builds.
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" core install "adafruit:samd@$(ADAFRUIT_SAMD_VERSION)"

arduino-install-libs: arduino-update-index ## Install pinned libraries for repository sketches.
	@set -eu; \
	while IFS= read -r lib; do \
		case "$$lib" in ""|\#*) continue ;; esac; \
		printf 'arduino-cli lib install %s\n' "$$lib"; \
		"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" lib install "$$lib"; \
	done < "$(ARDUINO_LIBRARIES_FILE)"

arduino-install-matrixportal-libs: arduino-update-index ## Install optional MatrixPortal guide libraries.
	@set -eu; \
	while IFS= read -r lib; do \
		case "$$lib" in ""|\#*) continue ;; esac; \
		printf 'arduino-cli lib install %s\n' "$$lib"; \
		"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" lib install "$$lib"; \
	done < "$(ARDUINO_MATRIXPORTAL_LIBRARIES_FILE)"

arduino-install: arduino-install-core arduino-install-libs ## Install board core and libraries needed to compile sketches.

arduino-test: arduino-compile ## Cross-compile all Arduino sketches without installing dependencies.

arduino-compile: arduino-check-cli ## Cross-compile all Arduino sketches, or SKETCH=path for one sketch.
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
			--build-path "$$build_path" \
			"$$sketch"; \
	done

arduino-ports: arduino-check-cli ## List connected Arduino serial ports.
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" board list

arduino-upload: arduino-check-cli ## Compile and upload SKETCH=path to PORT=/dev/cu.*.
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
		--build-path "$$build_path" \
		"$(SKETCH)"; \
	printf '==> upload %s to %s\n' "$(SKETCH)" "$(PORT)"; \
	"$(ARDUINO_CLI)" --config-file "$(ARDUINO_CONFIG)" upload \
		--fqbn "$(ARDUINO_FQBN)" \
		--port "$(PORT)" \
		--input-dir "$$build_path" \
		"$(SKETCH)"

sim-check-sdl: ## Verify SDL2 development tooling is available.
	@command -v "$(SDL2_CONFIG)" >/dev/null 2>&1 || { \
		echo "error: $(SDL2_CONFIG) is not installed or not on PATH"; \
		echo "on macOS, run: brew install sdl2"; \
		exit 127; \
	}

sim-build: sim-check-sdl ## Build SDL IDE with no SKETCH, or one sketch preview with SKETCH=path.
	@set -eu; \
	sketch="$(SIM_TARGET_SKETCH)"; \
	mkdir -p "$(SIM_BUILD_DIR)"; \
	if [ -z "$$sketch" ]; then \
		printf '==> SDL IDE\n'; \
		"$(SIM_CXX)" $(SIM_CPPFLAGS) $(SIM_CXXFLAGS) $$("$(SDL2_CONFIG)" --cflags) \
			$(SIM_IDE_SOURCES) $$("$(SDL2_CONFIG)" --libs) \
			-o "$(SIM_IDE_BIN)"; \
	else \
		test -f "$$sketch" || { echo "error: sketch not found: $$sketch"; exit 2; }; \
		name=$$(basename "$$(dirname "$$sketch")"); \
		bin="$(SIM_BUILD_DIR)/$$name-sim"; \
		printf '==> %s\n' "$$sketch"; \
		"$(SIM_CXX)" $(SIM_CPPFLAGS) $(SIM_CXXFLAGS) $$("$(SDL2_CONFIG)" --cflags) \
			-DSKETCH_SOURCE="\"$(abspath $(SIM_TARGET_SKETCH))\"" \
			-DSKETCH_NAME="\"$(SIM_TARGET_SKETCH)\"" \
			$(SIM_SOURCES) $$("$(SDL2_CONFIG)" --libs) \
			-o "$$bin"; \
	fi

sim-build-all: sim-check-sdl ## Build SDL preview apps for every Arduino sketch.
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
		"$(SIM_CXX)" $(SIM_CPPFLAGS) $(SIM_CXXFLAGS) $$("$(SDL2_CONFIG)" --cflags) \
			-DSKETCH_SOURCE="\"$(abspath .)/$$sketch\"" \
			-DSKETCH_NAME="\"$$sketch\"" \
			$(SIM_SOURCES) $$("$(SDL2_CONFIG)" --libs) \
			-o "$$bin"; \
	done

sim-run: sim-build ## Run SDL IDE with no SKETCH, or run one preview with SKETCH=path.
	@if [ -z "$(SIM_TARGET_SKETCH)" ]; then \
		"$(SIM_IDE_BIN)" --scale "$(SIM_SCALE)" --panel "$(SIM_PANEL)" --layout "$(SIM_LAYOUT)"; \
	else \
		"$(SIM_BIN)" --scale "$(SIM_SCALE)" --max-frames "$(SIM_MAX_FRAMES)" --panel "$(SIM_PANEL)" --layout "$(SIM_LAYOUT)"; \
	fi

sim-test: sim-build sim-build-all ## Compile the SDL IDE and every preview app without launching a window.

arduino-clean: ## Remove Arduino build outputs.
	rm -rf "$(ARDUINO_BUILD_DIR)"

sim-clean: ## Remove SDL preview build outputs.
	rm -rf "$(SIM_BUILD_DIR)"

clean: arduino-clean sim-clean ## Remove generated build outputs.

distclean: clean ## Remove generated build outputs, local tools, and local Arduino package data.
	rm -rf "$(TOOLS_DIR)" .arduino
