CC := clang
TARGET := aarch64-linux-gnu

CFLAGS := --target=$(TARGET) -march=armv8-a -Os -ffreestanding -fno-builtin \
	-fno-stack-protector -fno-pic -fno-pie -ffunction-sections -fdata-sections \
	-Wall -Wextra -Werror
LDFLAGS := --target=$(TARGET) -fuse-ld=lld -nostdlib -static \
	-Wl,--build-id=none -Wl,--gc-sections -Wl,-s -Wl,-z,noexecstack \
	-Wl,-z,max-page-size=4096 -Wl,-T,linker.ld

.PHONY: all clean

all: avbinit

avbinit: start.o avbinit.o linker.ld
	$(CC) $(LDFLAGS) start.o avbinit.o -o $@

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@

avbinit.o: avbinit.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f start.o avbinit.o avbinit
