KDIR ?=
OUT_DIR := $(CURDIR)/out

.PHONY: all module loader clean

all: module loader

module:
	@test -n "$(KDIR)" || { echo "错误：未设置 KDIR" >&2; exit 1; }
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)/module" modules
	mkdir -p "$(OUT_DIR)"
	cp -f module/avb_interceptor.ko "$(OUT_DIR)/avb_interceptor.ko"

loader:
	$(MAKE) -C loader
	mkdir -p "$(OUT_DIR)"
	cp -f loader/avbinit "$(OUT_DIR)/avbinit"

clean:
	@if test -n "$(KDIR)" && test -d "$(KDIR)"; then \
		$(MAKE) -C "$(KDIR)" M="$(CURDIR)/module" clean; \
	else \
		rm -f module/*.o module/*.ko module/*.mod module/*.mod.c module/*.mod.o \
			module/*.cmd module/Module.symvers module/modules.order; \
	fi
	$(MAKE) -C loader clean
	rm -f "$(OUT_DIR)/avb_interceptor.ko" "$(OUT_DIR)/avbinit" \
		"$(OUT_DIR)/patch-init-boot-android.sh" \
		"$(OUT_DIR)/magiskboot-arm64"
