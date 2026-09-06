# SPDX-License-Identifier: MIT
SAMPLES := 01-hello 02-checkerboard 03-counter 04-bounce 05-keypad 06-dino 07-relic-dive
.DEFAULT_GOAL := all
.PHONY: all run debug test clean $(SAMPLES)
all run debug test clean:
	@set -e; for sample in $(SAMPLES); do $(MAKE) -C "$$sample" $@; done
