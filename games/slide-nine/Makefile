# SPDX-License-Identifier: MIT
GAME := slide-nine
GAME_SOURCES := ../common/grid.s main.s
include ../common/game.mk

.PHONY: visual-test
visual-test: all
	"$(NODE)" check-visuals.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: visual-test
