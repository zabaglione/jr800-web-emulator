# SPDX-License-Identifier: MIT
GAME := mirror-link
include ../common/game.mk

assets.s: design.py

.PHONY: visual-test
visual-test: all
	"$(NODE)" check-visuals.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: visual-test
