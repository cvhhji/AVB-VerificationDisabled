TOOLKIT_NAME := toolkit_linux
PATCHER_SRC  := ../../abl_patcher
EXTRACTFV_SRC := build/extractfv_src
GBL_RAW := https://raw.githubusercontent.com/cvhhji/gbl_root_canoe/main

.PHONY: all clean patcher extractfv package

all: package

patcher:
	mkdir -p build/toolkit/bin
	$(MAKE) -C $(PATCHER_SRC)
	cp -f $(PATCHER_SRC)/patch_abl_avb build/toolkit/bin/patch_abl_avb

extractfv:
	mkdir -p build/toolkit/bin $(EXTRACTFV_SRC)
	curl -sL "$(GBL_RAW)/submodules/ablfvextractor/extractfv.c" -o $(EXTRACTFV_SRC)/extractfv.c
	gcc -O2 -o build/toolkit/bin/extractfv $(EXTRACTFV_SRC)/extractfv.c -llzma

package: patcher extractfv
	cp -r resources/* build/toolkit/
	cd build/toolkit && zip -r ../$(TOOLKIT_NAME).zip ./

clean:
	rm -rf build || true
