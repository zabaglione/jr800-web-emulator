# SPDX-License-Identifier: MIT
SAMPLE := 06-dino
SOURCES := main.s pcg.s
CHECK_SCRIPT := check.mjs
include ../common/sample.mk

.PHONY: clean-game
clean: clean-game
clean-game:
	rm -f "$(BUILD_DIR)/jump.svg" "$(BUILD_DIR)/game-over.svg" "$(BUILD_DIR)/pit.svg" \
		"$(BUILD_DIR)/browser-ready.png" "$(BUILD_DIR)/browser-jump.png" \
		"$(BUILD_DIR)/browser-pit.png" "$(BUILD_DIR)/browser-game-over.png" "$(BUILD_DIR)/browser-verification.json"

.PHONY: images
images:
	python3 render-sprites.py
