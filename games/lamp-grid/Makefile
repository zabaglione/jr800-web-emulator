# SPDX-License-Identifier: MIT
GAME := lamp-grid
GAME_SOURCES := ../common/grid.s main.s animation.s
include ../common/game.mk

.PHONY: animation-test
animation-test: all
	"$(NODE)" check-animation.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: animation-test
