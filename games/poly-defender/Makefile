# SPDX-License-Identifier: MIT
GAME := poly-defender
ROOT := $(abspath ../..)
SDK := $(ROOT)/sdk/lib
COMMON := $(ROOT)/sdk/examples/lcd/common
JR8AS ?= $(ROOT)/build/native-release/tools/jr8as
JR8LD ?= $(ROOT)/build/native-release/tools/jr8ld
NODE ?= node
PYTHON ?= python3
WASM_DIR ?= $(ROOT)/build/wasm-release/web-module
BUILD_DIR ?= $(ROOT)/build/games/$(GAME)
.DEFAULT_GOAL := all
.PHONY: all run debug test clean check-models
# Preserve the validated program layout and object order.
OBJECTS := $(addprefix $(BUILD_DIR)/,main.jro audio.jro sound.jro p3-render.jro p3-models.jro p3-runtime.jro game-input.jro dirty.jro display.jro font.jro basic.jro)

$(BUILD_DIR):
	mkdir -p "$@"
$(BUILD_DIR)/%.jro: %.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/$*.lst" -o "$@" "$<"
$(BUILD_DIR)/%.jro: $(SDK)/%.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/$*.lst" -o "$@" "$<"
$(BUILD_DIR)/%.jro: $(SDK)/lcd/%.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/$*.lst" -o "$@" "$<"
$(BUILD_DIR)/p3-render.jro: $(SDK)/poly3d/render.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/p3-render.lst" -o "$@" "$<"
$(BUILD_DIR)/p3-models.jro: $(SDK)/poly3d/models.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/p3-models.lst" -o "$@" "$<"
$(BUILD_DIR)/p3-runtime.jro: $(COMMON)/poly3d-runtime.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/p3-runtime.lst" -o "$@" "$<"
$(BUILD_DIR)/$(GAME).j8a: $(OBJECTS) $(COMMON)/poly3d-memory.j8l
	"$(JR8LD)" --script "$(COMMON)/poly3d-memory.j8l" -o "$@" \
		--debug "$(BUILD_DIR)/$(GAME).j8d" --map "$(BUILD_DIR)/$(GAME).map" \
		--symbols "$(BUILD_DIR)/$(GAME).sym" $(OBJECTS)
all: $(BUILD_DIR)/$(GAME).j8a
run debug test: all
	"$(NODE)" check.mjs "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" "$@"
check-models:
	$(PYTHON) "$(SDK)/poly3d/generate_models.py" --check
test: check-models
clean:
	$(PYTHON) -c 'import shutil; shutil.rmtree("$(BUILD_DIR)", ignore_errors=True)'
