# SPDX-License-Identifier: MIT
SAMPLE := 07-relic-dive
SOURCES := game.s
CHECK_SCRIPT := check.mjs
LINK_SCRIPT := memory.j8l
include ../common/sample.mk

MODULES := constants.inc main.s dungeon.s turns.s items.s ui.s assets.s
$(BUILD_DIR)/game.jro: $(MODULES) | $(BUILD_DIR)
	cat $(MODULES) > "$(BUILD_DIR)/game.s"
	"$(JR8AS)" --target hd6301v1 --listing "$(BUILD_DIR)/game.lst" -o "$@" "$(BUILD_DIR)/game.s"

.PHONY: clean-game
clean: clean-game
clean-game:
	rm -f "$(BUILD_DIR)/game.s" "$(BUILD_DIR)"/*.svg "$(BUILD_DIR)"/browser-*.png "$(BUILD_DIR)"/browser-*.json

.PHONY: images
images:
	python3 render-sprites.py
