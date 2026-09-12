# SPDX-License-Identifier: MIT
GAME := switch-maze
GAME_SOURCES := ../common/grid.s ../common/grid-undo.s main.s
include ../common/game.mk

.PHONY: intro-test
intro-test: all
	"$(NODE)" check-intro.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: intro-test
