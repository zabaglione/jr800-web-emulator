JR8AS ?= jr8as
JR8LD ?= jr8ld
JR8RUN ?= jr8run
BUILD_DIR ?= build

.DEFAULT_GOAL := all

OBJECT := $(BUILD_DIR)/main.jro
APPLICATION := $(BUILD_DIR)/write-watch.j8a
DEBUG_INFO := $(BUILD_DIR)/write-watch.j8d
MAP := $(BUILD_DIR)/write-watch.map
SYMBOLS := $(BUILD_DIR)/write-watch.sym

.PHONY: all run run-to run-to-source run-to-symbol break-if step-over step-out trace debug test clean

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJECT): main.s | $(BUILD_DIR)
	$(JR8AS) --target hd6301v1 -o $(OBJECT) main.s

all: $(OBJECT) memory.j8l
	$(JR8LD) --script memory.j8l -o $(APPLICATION) \
		--debug $(DEBUG_INFO) --map $(MAP) --symbols $(SYMBOLS) $(OBJECT)

run: all
	$(JR8RUN) --max-instructions 10 --dump 0x0000:2 \
		--expect-stop instruction-limit \
		--expect 'mem8[0x0000] == 0x42 && mem8[0x0001] == 0x99' $(APPLICATION)

run-to: all
	$(JR8RUN) --run-to 0x0207 --max-instructions 100 \
		--expect-stop address-reached --expect 'mem8[0x0000] == 0x42' $(APPLICATION)

run-to-source: all
	$(JR8RUN) --debug $(DEBUG_INFO) --run-to-source main.s:9 \
		--max-instructions 100 --expect-stop address-reached \
		--expect 'mem8[0x0000] == 0x42' $(APPLICATION)

run-to-symbol: all
	$(JR8RUN) --debug $(DEBUG_INFO) --run-to-symbol loop \
		--max-instructions 100 --expect-stop address-reached \
		--expect 'mem8[0x0000] == 0x42 && mem8[0x0001] == 0x99' $(APPLICATION)

break-if: all
	$(JR8RUN) --break-if '0x020a:A == 0x99 && mem8[0x0001] == 0x99' \
		--max-instructions 100 --expect-stop execution-breakpoint \
		--expect 'mem8[0x0000] == 0x42 && mem8[0x0001] == 0x99' $(APPLICATION)

step-over: all
	$(JR8RUN) --step-over --max-instructions 100 \
		--expect-stop step-complete --expect 'mem8[0x0000] == 0' $(APPLICATION)

step-out: all
	$(JR8RUN) --step-out --max-instructions 10 \
		--expect-stop instruction-limit --expect 'mem8[0x0000] == 0x42' $(APPLICATION)

trace: all
	$(JR8RUN) --trace write:0x0000:0x0000 --max-instructions 10 \
		--expect-stop instruction-limit --expect 'mem8[0x0000] == 0x42' $(APPLICATION)

debug: all
	$(JR8RUN) --debug $(DEBUG_INFO) --watch write:0x0001 \
		--max-instructions 100 --dump 0x0000:2 $(APPLICATION)

test: all
	$(JR8RUN) --debug $(DEBUG_INFO) --watch write:0x0001 \
		--max-instructions 100 --expect-stop memory-watchpoint \
		--expect 'symbol("loop") == 0x020A' \
		--expect 'PC == 0x020A && SP == 0x01FF && X == 0' \
		--expect 'A == 0x99 && B == 0 && CCR == 0xC8 && cycles == 12' \
		--expect 'mem8[0x0000] == 0x42 && mem8[0x0001] == 0x99' $(APPLICATION)

clean:
	rm -f $(OBJECT) $(APPLICATION) $(DEBUG_INFO) $(MAP) $(SYMBOLS)
