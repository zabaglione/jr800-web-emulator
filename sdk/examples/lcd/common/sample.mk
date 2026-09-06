# SPDX-License-Identifier: MIT
ROOT := $(abspath ../../../..)
JR8AS ?= $(ROOT)/build/native-release/tools/jr8as
JR8LD ?= $(ROOT)/build/native-release/tools/jr8ld
NODE ?= node
WASM_DIR ?= $(ROOT)/build/wasm-release/web-module
BUILD_DIR ?= $(ROOT)/build/sdk-lcd/$(SAMPLE)
SOURCES ?= main.s
CHECK_SCRIPT ?= ../check.mjs
LINK_SCRIPT ?= ../common/memory.j8l
.DEFAULT_GOAL := all
.PHONY: all run debug test clean

OBJECTS := $(SOURCES:%.s=$(BUILD_DIR)/%.jro) $(BUILD_DIR)/display.jro $(BUILD_DIR)/font.jro $(BUILD_DIR)/basic.jro

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/%.jro: %.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/$*.lst" -o "$@" "$<"

$(BUILD_DIR)/%.jro: ../common/%.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/$*.lst" -o "$@" "$<"

all: $(OBJECTS) $(LINK_SCRIPT)
	"$(JR8LD)" --script "$(LINK_SCRIPT)" -o "$(BUILD_DIR)/$(SAMPLE).j8a" \
		--debug "$(BUILD_DIR)/$(SAMPLE).j8d" --map "$(BUILD_DIR)/$(SAMPLE).map" \
		--symbols "$(BUILD_DIR)/$(SAMPLE).sym" $(OBJECTS)

run debug test: all
	"$(NODE)" "$(CHECK_SCRIPT)" "$(WASM_DIR)" "$(BUILD_DIR)" "$(SAMPLE)" "$@"

clean:
	rm -f $(OBJECTS) "$(BUILD_DIR)"/*.lst "$(BUILD_DIR)/$(SAMPLE).j8a" \
		"$(BUILD_DIR)/$(SAMPLE).j8d" "$(BUILD_DIR)/$(SAMPLE).map" \
		"$(BUILD_DIR)/$(SAMPLE).sym" "$(BUILD_DIR)/screen.svg" "$(BUILD_DIR)/verification.json"
