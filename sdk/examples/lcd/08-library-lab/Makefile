SAMPLE := 08-library-lab
SOURCES := main.s keys.s sprite.s scroll.s sound.s
CHECK_SCRIPT := check.mjs
include ../common/sample.mk
# This sample requires BASIC return support. Keep exactly one copy.
OBJECTS := $(filter-out $(BUILD_DIR)/basic.jro,$(OBJECTS)) $(BUILD_DIR)/basic.jro
all: $(BUILD_DIR)/basic.jro
$(BUILD_DIR)/%.jro: ../../../lib/%.s | $(BUILD_DIR)
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/$*.lst" -o "$@" "$<"
