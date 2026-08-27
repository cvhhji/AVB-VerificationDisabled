 SPDX-License-Identifier: GPL-2.0-only
# Makefile for BL-stage ABL AVB patcher

CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
SRCS    := patch_abl_avb.c avb_patch.c arm64_util.c
OBJS    := $(SRCS:.c=.o)
TARGET  := patch_abl_avb

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
