# SPDX-License-Identifier: MIT
ROOT := $(abspath ../..)
JR8AS ?= $(ROOT)/build/native-release/tools/jr8as
JR8LD ?= $(ROOT)/build/native-release/tools/jr8ld
NODE ?= node
PYTHON ?= python3
WASM_DIR ?= $(ROOT)/build/wasm-release/web-module
BUILD_DIR ?= $(ROOT)/build/games/$(GAME)
.DEFAULT_GOAL := all
COMMON := $(ROOT)/games/common
SDK := $(ROOT)/sdk/lib
GAME_SOURCES ?= main.s
OBJECTS := $(addprefix $(BUILD_DIR)/,game.jro display.jro font.jro basic.jro dirty.jro input.jro sound.jro)
.PHONY: all run debug test clean
assets.s: generate.py $(wildcard $(ROOT)/games/tools/*.py) $(SDK)/lcd/font.s
	$(PYTHON) generate.py
$(BUILD_DIR):
	mkdir -p "$@"
visuals.s: $(ROOT)/games/tools/hud_assets.py $(ROOT)/games/tools/hud_layouts.py $(ROOT)/games/tools/hud_custom.py $(ROOT)/games/tools/visual_art.py $(SDK)/lcd/font.s
	$(PYTHON) $(ROOT)/games/tools/hud_assets.py $(GAME)
$(BUILD_DIR)/game.s: $(GAME_SOURCES) assets.s visuals.s $(COMMON)/runtime.s $(COMMON)/graphics.s $(COMMON)/visual-hud.s $(COMMON)/game.mk | $(BUILD_DIR)
	cat visuals.s $(COMMON)/runtime.s $(COMMON)/graphics.s $(COMMON)/visual-hud.s $(GAME_SOURCES) assets.s > "$@"
$(BUILD_DIR)/game.jro: $(BUILD_DIR)/game.s
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/game.lst" -o "$@" "$<"
$(BUILD_DIR)/%.jro: $(SDK)/lcd/%.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 -o "$@" "$<"
$(BUILD_DIR)/input.jro: $(SDK)/game-input.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 -o "$@" "$<"
$(BUILD_DIR)/sound.jro: $(SDK)/sound.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 -o "$@" "$<"
$(BUILD_DIR)/$(GAME).j8a: $(OBJECTS) $(COMMON)/memory.j8l
	"$(JR8LD)" --script "$(COMMON)/memory.j8l" -o "$@" --debug "$(BUILD_DIR)/$(GAME).j8d" --map "$(BUILD_DIR)/$(GAME).map" --symbols "$(BUILD_DIR)/$(GAME).sym" $(OBJECTS)
all: $(BUILD_DIR)/$(GAME).j8a
run debug test: all
	"$(NODE)" "$(ROOT)/games/tools/check.mjs" "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" "$@"
clean:
	$(PYTHON) -c 'import shutil; shutil.rmtree("$(BUILD_DIR)", ignore_errors=True)'
