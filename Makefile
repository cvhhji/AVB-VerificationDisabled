SHELL := /bin/bash
OUT_DIR := $(CURDIR)/out

.PHONY: all abl_patcher test clean

all: abl_patcher

abl_patcher:
	$(MAKE) -C abl_patcher
	mkdir -p "$(OUT_DIR)"
	cp -f abl_patcher/patch_abl_avb "$(OUT_DIR)/patch_abl_avb"

test: abl_patcher
	@bash tests/test_abl_patch.sh abl_patcher/patch_abl_avb tests/samples

clean:
	$(MAKE) -C abl_patcher clean
	rm -f "$(OUT_DIR)/patch_abl_avb"
