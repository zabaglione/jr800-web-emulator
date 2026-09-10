# SPDX-License-Identifier: MIT
GAMES := box-shift mirror-link step-strike circuit-deck pocket-factory arc-duel
.DEFAULT_GOAL := all
.PHONY: all run debug test clean $(GAMES)
all run debug test clean:
	@set -e; for game in $(GAMES); do $(MAKE) --no-print-directory -C $$game $@; done
$(GAMES):
	$(MAKE) --no-print-directory -C $@
