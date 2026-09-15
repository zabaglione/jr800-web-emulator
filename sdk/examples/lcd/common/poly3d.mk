# SPDX-License-Identifier: MIT
SOURCES := main.s p3-render.s p3-models.s p3-runtime.s game-input.s dirty.s
CHECK_SCRIPT := check.mjs
LINK_SCRIPT := ../common/poly3d-memory.j8l
PYTHON ?= python3
include ../common/sample.mk

$(BUILD_DIR)/p3-render.jro: $(ROOT)/sdk/lib/poly3d/render.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/p3-render.lst" -o "$@" "$<"
$(BUILD_DIR)/p3-models.jro: $(ROOT)/sdk/lib/poly3d/models.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/p3-models.lst" -o "$@" "$<"
$(BUILD_DIR)/p3-runtime.jro: ../common/poly3d-runtime.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/p3-runtime.lst" -o "$@" "$<"
$(BUILD_DIR)/game-input.jro: $(ROOT)/sdk/lib/game-input.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/game-input.lst" -o "$@" "$<"

.PHONY: check-models
check-models:
	$(PYTHON) "$(ROOT)/sdk/lib/poly3d/generate_models.py" --check
test: check-models
