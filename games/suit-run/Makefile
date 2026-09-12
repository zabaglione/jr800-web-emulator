# SPDX-License-Identifier: MIT
GAME := suit-run
GAME_SOURCES := ../common/grid.s ../common/grid-undo.s main.s
include ../common/game.mk

.PHONY: actions-test
actions-test: all
	"$(NODE)" check-actions.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: actions-test
