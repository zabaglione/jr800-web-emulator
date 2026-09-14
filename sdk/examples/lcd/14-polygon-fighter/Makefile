# SPDX-License-Identifier: MIT
SAMPLE := 14-polygon-fighter
SOURCES := main.s transfer.s spans.s renderer.s model.s
CHECK_SCRIPT := check.mjs
LINK_SCRIPT := memory.j8l
PYTHON ?= python3
include ../common/sample.mk

model.s: generate_model.py
	$(PYTHON) generate_model.py

.PHONY: check-model clean-extra test-busy
check-model:
	$(PYTHON) generate_model.py --check
test: check-model
clean: clean-extra
clean-extra:
	rm -f "$(BUILD_DIR)"/rotation*.svg "$(BUILD_DIR)"/rotation*.json "$(BUILD_DIR)/run.json" "$(BUILD_DIR)/debug.json"
	rm -f "$(BUILD_DIR)"/frame-??.png "$(BUILD_DIR)/contact-sheet.png" "$(BUILD_DIR)/rotation.gif"
	rm -f "$(BUILD_DIR)/verification-owned-rom.json"

# Optional Native protocol fixture; make test remains the WASM sample check.
BUSY_TEST ?= $(ROOT)/build/native-release/tests/lcd_sample_busy_test
test-busy:
	"$(BUSY_TEST)" "$(ROOT)/sdk/lib/lcd/display.s" "$(ROOT)/sdk/lib/lcd/font.s" \
		"$(ROOT)/games/common/memory.j8l" dirty transfer.s
