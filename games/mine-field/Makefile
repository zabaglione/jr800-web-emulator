# SPDX-License-Identifier: MIT
GAME := mine-field
GAME_SOURCES := ../common/grid.s main.s
include ../common/game.mk

.PHONY: actions-test
actions-test: all
	"$(NODE)" check-actions.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: actions-test
