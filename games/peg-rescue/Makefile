# SPDX-License-Identifier: MIT
GAME := peg-rescue
GAME_SOURCES := ../common/grid.s ../common/grid-undo.s main.s
include ../common/game.mk

.PHONY: presentation-test
presentation-test: all
	"$(NODE)" "$(ROOT)/games/tools/presentation_check.mjs" "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" focus
test: presentation-test
