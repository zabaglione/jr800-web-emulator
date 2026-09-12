# SPDX-License-Identifier: MIT
GAME := lamp-grid
GAME_SOURCES := ../common/grid.s main.s animation.s
include ../common/game.mk

assets.s: hud.py title.py

.PHONY: animation-test
animation-test: all
	"$(NODE)" check-animation.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: animation-test

.PHONY: presentation-test
presentation-test: all
	"$(NODE)" "$(ROOT)/games/tools/presentation_check.mjs" "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" focus
test: presentation-test
