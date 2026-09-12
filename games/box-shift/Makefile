# SPDX-License-Identifier: MIT
GAME := box-shift
include ../common/game.mk

.PHONY: intro-test
intro-test: all
	"$(NODE)" check-intro.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: intro-test
