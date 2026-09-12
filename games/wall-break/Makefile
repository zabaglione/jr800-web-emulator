# SPDX-License-Identifier: MIT
GAME := wall-break
GAME_SOURCES := ../common/scene.s main.s
include ../common/game.mk

.PHONY: presentation-test
presentation-test: all
	"$(NODE)" "$(ROOT)/games/tools/presentation_check.mjs" "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" intro
test: presentation-test
