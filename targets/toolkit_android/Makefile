TOOLKIT_NAME := toolkit_android
PATCHER_SRC  := ../../abl_patcher
EXTRACTFV_SRC := build/extractfv_src
GBL_RAW := https://raw.githubusercontent.com/cvhhji/gbl_root_canoe/main
CC_ANDROID := aarch64-linux-gnu-gcc

.PHONY: all clean patcher extractfv package

all: package

patcher:
	mkdir -p build/toolkit/bin
	$(CC_ANDROID) -O2 -static -o build/toolkit/bin/patch_abl_avb \
		$(PATCHER_SRC)/patch_abl_avb.c $(PATCHER_SRC)/avb_patch.c $(PATCHER_SRC)/arm64_util.c

extractfv:
	mkdir -p build/toolkit/bin $(EXTRACTFV_SRC)
	curl -sL "$(GBL_RAW)/submodules/ablfvextractor/extractfv.c" -o $(EXTRACTFV_SRC)/extractfv.c
	$(CC_ANDROID) -O2 -static -o build/toolkit/bin/extractfv $(EXTRACTFV_SRC)/extractfv.c -llzma

package: patcher extractfv
	cp -r resources/* build/toolkit/
	cd build/toolkit && zip -r ../$(TOOLKIT_NAME).zip ./

clean:
	rm -rf build || true
