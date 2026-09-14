# SPDX-License-Identifier: MIT
SAMPLE := 14-polygon-fighter
SOURCES := main.s renderer.s model.s
CHECK_SCRIPT := check.mjs
LINK_SCRIPT := memory.j8l
PYTHON ?= python3
include ../common/sample.mk

model.s: generate_model.py
	$(PYTHON) generate_model.py

.PHONY: check-model clean-extra
check-model:
	$(PYTHON) generate_model.py --check
test: check-model
clean: clean-extra
clean-extra:
	rm -f "$(BUILD_DIR)"/rotation*.svg "$(BUILD_DIR)"/rotation*.json "$(BUILD_DIR)/run.json" "$(BUILD_DIR)/debug.json"
	rm -f "$(BUILD_DIR)"/frame-??.png "$(BUILD_DIR)/contact-sheet.png" "$(BUILD_DIR)/rotation.gif"
	rm -f "$(BUILD_DIR)/verification-owned-rom.json"
