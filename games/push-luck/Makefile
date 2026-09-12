# SPDX-License-Identifier: MIT
GAME := push-luck
include ../common/game.mk
.PHONY: effects-test
effects-test: all
	"$(NODE)" check-effects.mjs "$(WASM_DIR)" "$(BUILD_DIR)"
test: effects-test

.PHONY: presentation-test
presentation-test: all
	"$(NODE)" "$(ROOT)/games/tools/presentation_check.mjs" "$(WASM_DIR)" "$(BUILD_DIR)" "$(GAME)" focus
test: presentation-test
