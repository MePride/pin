#
# Pin Firmware Makefile
#

PROJECT_NAME := pin_firmware

# ESP-IDF Configuration
IDF_PATH ?= $(shell echo $$IDF_PATH)
IDF_TARGET := esp32c3

# Guard: ensure ESP-IDF is available
ifeq ($(strip $(IDF_PATH)),)
$(error IDF_PATH is not set. Install ESP-IDF v5.1+ and run 'source <esp-idf>/export.sh' to populate the environment.)
endif

# Source directories
MAIN_DIR := main
COMPONENTS_DIR := components
PLUGINS_DIR := plugins
WEB_DIR := web

# Build configuration
BUILD_TYPE ?= debug
MONITOR_PORT ?= /dev/ttyUSB0
MONITOR_BAUD := 115200

# Include ESP-IDF build system
include $(IDF_PATH)/tools/cmake/project.mk

# Custom targets
.PHONY: web-build web-embed flash-web monitor-full clean-all help env-check lint docs

# Build web assets
web-build:
	@echo "Building web assets..."
	@cd $(WEB_DIR) && \
	echo "Minifying HTML..." && \
	cp index.html index.min.html && \
	echo "Minifying JavaScript..." && \
	cp app.js app.min.js && \
	echo "Web assets built successfully"

# Embed web assets into firmware
web-embed: web-build
	@echo "Embedding web assets..."
	@mkdir -p build/embedded_files
	@cd $(WEB_DIR) && \
	xxd -i index.html > ../build/embedded_files/index_html.c && \
	xxd -i app.js > ../build/embedded_files/app_js.c && \
	xxd -i sw.js > ../build/embedded_files/sw_js.c && \
	xxd -i manifest.json > ../build/embedded_files/manifest_json.c
	@echo "Web assets embedded successfully"

# Flash web assets to SPIFFS
flash-web: web-embed
	@echo "Creating SPIFFS image..."
	@$(IDF_PATH)/components/spiffs/spiffsgen.py 0x70000 $(WEB_DIR) build/spiffs.bin
	@echo "Flashing SPIFFS..."
	@esptool.py --chip esp32c3 --port $(MONITOR_PORT) write_flash 0x190000 build/spiffs.bin
	@echo "Web assets flashed successfully"

# Enhanced monitor with plugin logs
monitor-full:
	@echo "Starting enhanced monitor..."
	@idf.py monitor --port $(MONITOR_PORT) --baud $(MONITOR_BAUD)

# Clean everything including web build
clean-all: clean
	@echo "Cleaning web assets..."
	@rm -rf $(WEB_DIR)/*.min.*
	@rm -rf build/embedded_files
	@rm -f build/spiffs.bin
	@echo "Clean completed"

# Build and flash everything
all: web-embed build flash flash-web

# Development helpers
dev: build flash monitor-full

# Help target
help:
	@echo "Pin Firmware Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  build         - Build firmware"
	@echo "  flash         - Flash firmware to device"
	@echo "  monitor       - Start serial monitor"
	@echo "  web-build     - Build web assets"
	@echo "  web-embed     - Embed web assets into firmware"
	@echo "  flash-web     - Flash web assets to SPIFFS"
	@echo "  monitor-full  - Enhanced monitor with plugin logs"
	@echo "  clean-all     - Clean everything"
	@echo "  env-check     - Verify ESP-IDF toolchain and prereqs"
	@echo "  all          - Build and flash everything"
	@echo "  dev          - Quick development cycle"
	@echo ""
	@echo "Configuration:"
	@echo "  MONITOR_PORT  - Serial port (default: $(MONITOR_PORT))"
	@echo "  BUILD_TYPE    - Build type: debug/release (default: $(BUILD_TYPE))"

# Environment check helper
env-check:
	@bash ../tools/check_env.sh

# Optional convenience targets (stubbed here)
lint:
	@echo "No lint configured in Makefile. Use 'idf.py clang-tidy' or project-specific tools if available."

docs:
	@echo "No docs build configured. Generate with Doxygen or project scripts if present."
