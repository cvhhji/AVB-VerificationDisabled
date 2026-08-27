TOOLKIT_NAME := toolkit_windows
PATCHER_SRC  := ../../abl_patcher
EXTRACTFV_SRC := build/extractfv_src
GBL_RAW := https://raw.githubusercontent.com/cvhhji/gbl_root_canoe/main
CC_WIN := x86_64-w64-mingw32-gcc

.PHONY: all clean patcher extractfv package

all: package

patcher:
	mkdir -p build/toolkit/bin
	$(CC_WIN) -O2 -o build/toolkit/bin/patch_abl_avb.exe \
		$(PATCHER_SRC)/patch_abl_avb.c $(PATCHER_SRC)/avb_patch.c $(PATCHER_SRC)/arm64_util.c

extractfv:
	mkdir -p build/toolkit/bin $(EXTRACTFV_SRC)
	curl -sL "$(GBL_RAW)/submodules/ablfvextractor/extractfv.c" -o $(EXTRACTFV_SRC)/extractfv.c
	$(CC_WIN) -O2 -o build/toolkit/bin/extractfv.exe $(EXTRACTFV_SRC)/extractfv.c -llzma

package: patcher extractfv
	cp -r resources/* build/toolkit/
	cd build/toolkit && zip -r ../$(TOOLKIT_NAME).zip ./

clean:
	rm -rf build || true
