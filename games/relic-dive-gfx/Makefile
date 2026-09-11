# SPDX-License-Identifier: MIT
GAME := relic-dive-gfx
ROOT := $(abspath ../..)
SDK := $(ROOT)/sdk/lib/lcd
ORIGINAL := $(ROOT)/sdk/examples/lcd/07-relic-dive
BUILD_DIR ?= $(ROOT)/build/games/$(GAME)
JR8AS ?= $(ROOT)/build/native-release/tools/jr8as
JR8LD ?= $(ROOT)/build/native-release/tools/jr8ld
WASM_DIR ?= $(ROOT)/build/wasm-release/web-module
NODE ?= node
PYTHON ?= python3
.DEFAULT_GOAL := all
OBJECTS := $(addprefix $(BUILD_DIR)/,game.jro display.jro font.jro basic.jro)
.PHONY: all run debug test clean
$(BUILD_DIR):
	mkdir -p "$@"
assets.s: generate.py $(ROOT)/games/tools/art.py $(ROOT)/games/tools/depth_art.py
	$(PYTHON) generate.py
$(BUILD_DIR)/game.s: prepare.py assets.s graphics.s letter-input.s $(wildcard $(ORIGINAL)/*.s) $(ORIGINAL)/constants.inc $(SDK)/dirty.s $(ROOT)/sdk/lib/sound.s $(ROOT)/games/tools/puzzle_assets.py | $(BUILD_DIR)
	$(PYTHON) prepare.py "$@"
$(BUILD_DIR)/game.jro: $(BUILD_DIR)/game.s
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/game.lst" -o "$@" "$<"
$(BUILD_DIR)/%.jro: $(SDK)/%.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 -o "$@" "$<"
$(BUILD_DIR)/$(GAME).j8a: $(OBJECTS) memory.j8l
	"$(JR8LD)" --script memory.j8l -o "$@" --debug "$(BUILD_DIR)/$(GAME).j8d" --map "$(BUILD_DIR)/$(GAME).map" --symbols "$(BUILD_DIR)/$(GAME).sym" $(OBJECTS)
all: $(BUILD_DIR)/$(GAME).j8a
run debug test: all
	"$(NODE)" check.mjs "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" "$@"
clean:
	$(PYTHON) -c 'import shutil; shutil.rmtree("$(BUILD_DIR)", ignore_errors=True)'
