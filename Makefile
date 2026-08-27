DIR ?=
OUT_DIR := $(CURDIR)/out

.PHONY: all module loader abl_patcher clean

all: module loader abl_patcher

module:
	@test -n "$(KDIR)" || { echo "??:??? KDIR,??? tools/build.sh --target <DDK target> ??" >&2; exit 1; }
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)/module" modules
	mkdir -p "$(OUT_DIR)"
	cp -f module/avb_interceptor.ko "$(OUT_DIR)/avb_interceptor.ko"

loader:
	$(MAKE) -C loader
	mkdir -p "$(OUT_DIR)"
	cp -f loader/avbinit "$(OUT_DIR)/avbinit"

# BL-stage ABL patcher (host tool, no KDIR needed)
abl_patcher:
	$(MAKE) -C abl_patcher
	mkdir -p "$(OUT_DIR)"
	cp -f abl_patcher/patch_abl_avb "$(OUT_DIR)/patch_abl_avb"

clean:
	@if test -n "$(KDIR)" && test -d "$(KDIR)"; then \
		$(MAKE) -C "$(KDIR)" M="$(CURDIR)/module" clean; \
	else \
		rm -f module/*.o module/*.ko module/*.mod module/*.mod.c module/*.mod.o \
			module/*.cmd module/Module.symvers module/modules.order; \
	fi
	$(MAKE) -C loader clean
	$(MAKE) -C abl_patcher clean
	rm -f "$(OUT_DIR)/avb_interceptor.ko" "$(OUT_DIR)/avbinit" \
		"$(OUT_DIR)/patch_abl_avb" \
		"$(OUT_DIR)/patch-init-boot-android.sh" \
		"$(OUT_DIR)/magiskboot-arm64"
