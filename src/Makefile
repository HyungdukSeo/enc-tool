# libUEnc - audio file AES-256-CBC encryption library
#
# Build:
#   make                              -> Linux3.10.0_ICC/libUEnc.a
#   make install                      -> ../lib/$(ARCH)/libUEnc.a + ../include/UEnc.h
#   make clean
#
# Variables (override on the command line):
#   ARCH       Target subdir (default: Linux3.10.0_ICC, matches lib/ layout)
#   CC         Compiler   (default: gcc, set CC=icc for Intel CC if available)
#   CFLAGS     Extra C flags

OUTPUT_FILE  = libUEnc.a
ARCH        ?= Linux3.10.0_ICC
INSTALL_DIR  = ../lib/$(ARCH)
INCLUDE_DIR  = ../include

SRCS         = UEnc.c
HDRS         = UEnc.h
OBJS         = $(SRCS:%.c=$(ARCH)/%.o)

CC          ?= gcc
AR          ?= ar
RANLIB      ?= ranlib

# OpenSSL: prefer pkg-config when available, fall back to system defaults.
OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null)

CFLAGS      ?= -O2 -g -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
CFLAGS      += $(OPENSSL_CFLAGS) -I.

.PHONY: all clean install

all: $(ARCH)/$(OUTPUT_FILE)

$(ARCH)/$(OUTPUT_FILE): $(OBJS)
	$(AR) cru $@ $^
	$(RANLIB) $@

$(ARCH)/%.o: %.c $(HDRS)
	@mkdir -p $(ARCH)
	$(CC) $(CFLAGS) -c $< -o $@

install: $(ARCH)/$(OUTPUT_FILE)
	@mkdir -p $(INSTALL_DIR)
	@mkdir -p $(INCLUDE_DIR)
	cp $(ARCH)/$(OUTPUT_FILE) $(INSTALL_DIR)/
	cp $(HDRS) $(INCLUDE_DIR)/

clean:
	rm -rf $(ARCH)
