# SPDX-License-Identifier: MIT
SAMPLE := 12-key-edges
SOURCES := main.s keys.s
CHECK_SCRIPT := ../library-samples-check.mjs
include ../common/sample.mk
# This sample requires BASIC return support. Keep exactly one copy.
OBJECTS := $(filter-out $(BUILD_DIR)/basic.jro,$(OBJECTS)) $(BUILD_DIR)/basic.jro
all: $(BUILD_DIR)/basic.jro
$(BUILD_DIR)/%.jro: ../../../lib/%.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/$*.lst" -o "$@" "$<"
.PHONY: clean-extra
clean: clean-extra
clean-extra:
	rm -f "$(BUILD_DIR)"/*.svg "$(BUILD_DIR)"/*.pixels.json "$(BUILD_DIR)"/*.png "$(BUILD_DIR)/run.json" "$(BUILD_DIR)/debug.json"
