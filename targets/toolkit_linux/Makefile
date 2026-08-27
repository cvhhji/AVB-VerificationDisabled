TOOLKIT_NAME := toolkit_linux
PATCHER_SRC  := ../../abl_patcher
PACKER_SRC   := ../../abl_packer
EXTRACTFV_SRC := build/extractfv_src
GBL_SRC := build/gbl_patcher
GBL_RAW := https://raw.githubusercontent.com/cvhhji/gbl_root_canoe/main
CC := gcc

GBL_SRCS := src/patch_abl.c src/patchs/core.c src/patchs/oplus/warning.c \
            src/patchs/oplus/forceenablefastboot.c \
            src/arm64_inst/arm64_inst_decoder.c src/arm64_inst/utils.c
GBL_INCLUDES := include/patchs/core.h include/patchs/oplus/warning.h \
                include/patchs/oplus/forceenablefastboot.h \
                include/arm64_inst/arm64_inst_decoder.h include/arm64_inst/utils.h

.PHONY: all clean patcher gbl_patcher packer extractfv package

all: package

patcher:
	mkdir -p build/toolkit/bin
	$(MAKE) -C $(PATCHER_SRC)
	cp -f $(PATCHER_SRC)/patch_abl_avb build/toolkit/bin/patch_abl_avb

gbl_patcher:
	mkdir -p build/toolkit/bin $(GBL_SRC)/src/patchs/oplus $(GBL_SRC)/src/arm64_inst \
		$(GBL_SRC)/include/patchs/oplus $(GBL_SRC)/include/arm64_inst
	@for f in $(GBL_SRCS) $(GBL_INCLUDES); do \
		curl -sL "$(GBL_RAW)/submodules/patcher/$$f" -o "$(GBL_SRC)/$$f"; \
	done
	$(CC) -O2 -I$(GBL_SRC)/include -o build/toolkit/bin/patch_abl \
		$(addprefix $(GBL_SRC)/,$(GBL_SRCS))

packer:
	mkdir -p build/toolkit/bin
	$(CC) -O2 -o build/toolkit/bin/abl_pack $(PACKER_SRC)/abl_pack.c

extractfv:
	mkdir -p build/toolkit/bin $(EXTRACTFV_SRC)
	curl -sL "$(GBL_RAW)/submodules/ablfvextractor/extractfv.c" -o $(EXTRACTFV_SRC)/extractfv.c
	$(CC) -O2 -o build/toolkit/bin/extractfv $(EXTRACTFV_SRC)/extractfv.c -llzma

package: patcher gbl_patcher packer extractfv
	cp -r resources/* build/toolkit/
	cd build/toolkit && zip -r ../$(TOOLKIT_NAME).zip ./

clean:
	rm -rf build || true
