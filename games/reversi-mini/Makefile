# SPDX-License-Identifier: MIT
GAME := reversi-mini
GAME_SOURCES := ../common/grid.s ../common/grid-undo.s main.s
include ../common/game.mk

.PHONY: animation-test
animation-test: all
	"$(NODE)" check-animation.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: animation-test
