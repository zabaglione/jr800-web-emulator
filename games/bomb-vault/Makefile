# SPDX-License-Identifier: MIT
GAME := bomb-vault
GAME_SOURCES := ../common/grid.s main.s
include ../common/game.mk

.PHONY: presentation-test
presentation-test: all
	"$(NODE)" "$(ROOT)/games/tools/presentation_check.mjs" "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" intro
test: presentation-test
