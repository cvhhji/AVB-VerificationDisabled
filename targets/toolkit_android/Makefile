TOOLKIT_NAME := toolkit_android
PATCHER_SRC  := ../../abl_patcher
EXTRACTFV_SRC := build/extractfv_src
LZMA_SRC := build/lzma_src
GBL_RAW := https://raw.githubusercontent.com/cvhhji/gbl_root_canoe/main
XZ_VERSION := 5.4.5
XZ_URL := https://tukaani.org/xz/xz-$(XZ_VERSION).tar.gz
CC_ANDROID := aarch64-linux-gnu-gcc

.PHONY: all clean patcher lzma extractfv package

all: package

patcher:
	mkdir -p build/toolkit/bin
	$(CC_ANDROID) -O2 -static -o build/toolkit/bin/patch_abl_avb \
		$(PATCHER_SRC)/patch_abl_avb.c $(PATCHER_SRC)/avb_patch.c $(PATCHER_SRC)/arm64_util.c

lzma:
	mkdir -p $(LZMA_SRC)
	if [ ! -f $(LZMA_SRC)/src/liblzma/.libs/liblzma.a ]; then \
		if [ -n "$(XZ_SRC)" ] && [ -f "$(XZ_SRC)/configure" ]; then \
			cp -r $(XZ_SRC)/* $(LZMA_SRC)/; \
		else \
			curl -sL --retry 3 --connect-timeout 30 "$(XZ_URL)" -o $(LZMA_SRC)/xz.tar.gz && \
			tar -xzf $(LZMA_SRC)/xz.tar.gz -C $(LZMA_SRC) --strip-components=1; \
		fi; \
		(cd $(LZMA_SRC) && ./configure --host=aarch64-linux-gnu \
			--enable-static --disable-shared --disable-nls \
			--disable-xz --disable-xzdec --disable-lzmadec \
			--disable-lzmainfo --disable-scripts --disable-doc \
			CFLAGS="-O2" && \
		$(MAKE) -j$$(nproc)); \
	fi

extractfv: lzma
	mkdir -p build/toolkit/bin $(EXTRACTFV_SRC)
	curl -sL "$(GBL_RAW)/submodules/ablfvextractor/extractfv.c" -o $(EXTRACTFV_SRC)/extractfv.c
	$(CC_ANDROID) -O2 -static -o build/toolkit/bin/extractfv \
		$(EXTRACTFV_SRC)/extractfv.c \
		-I$(LZMA_SRC)/src/liblzma/api \
		-L$(LZMA_SRC)/src/liblzma/.libs -llzma

package: patcher extractfv
	cp -r resources/* build/toolkit/
	cd build/toolkit && zip -r ../$(TOOLKIT_NAME).zip ./

clean:
	rm -rf build || true
