# SPDX-License-Identifier: MIT
GAME := loop-trace
GAME_SOURCES := ../common/grid.s main.s
include ../common/game.mk

assets.s: design.py hud.py

.PHONY: visual-test
visual-test: all
	"$(NODE)" check-visuals.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: visual-test
