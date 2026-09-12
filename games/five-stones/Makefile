# SPDX-License-Identifier: MIT
GAME := five-stones
GAME_SOURCES := ../common/grid.s ../common/grid-undo.s ../common/line-check.s main.s
include ../common/game.mk

.PHONY: presentation-test
presentation-test: all
	"$(NODE)" "$(ROOT)/games/tools/presentation_check.mjs" "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" focus
test: presentation-test
