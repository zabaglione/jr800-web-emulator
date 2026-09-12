# SPDX-License-Identifier: MIT
GAME := dice-hold
GAME_SOURCES := ../common/grid.s main.s
include ../common/game.mk
$(BUILD_DIR)/game.s: Makefile
.PHONY: actions-test
actions-test: all
	"$(NODE)" check-actions.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: actions-test

.PHONY: presentation-test
presentation-test: all
	"$(NODE)" "$(ROOT)/games/tools/presentation_check.mjs" "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" focus
test: presentation-test
