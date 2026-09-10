# SPDX-License-Identifier: MIT
GAMES := $(shell python3 -c 'import json; print(" ".join(p["id"] for p in json.load(open("catalog.json"))["programs"]))')
.DEFAULT_GOAL := all
.PHONY: all run debug test clean $(GAMES)
all run debug test clean:
	@set -e; for game in $(GAMES); do $(MAKE) --no-print-directory -C $$game $@; done
$(GAMES):
	$(MAKE) --no-print-directory -C $@
