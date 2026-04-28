# enc_tool.exe - audio file encryption CLI
#
# Build:
#   make                       -> ./enc_tool.exe (links ../lib/$(ARCH)/libUEnc.a)
#   make clean
#
# Variables (override on the command line):
#   ARCH       Library subdir (default: Linux3.10.0_ICC, matches lib/ layout)
#   CC         Compiler (default: gcc)
#   CFLAGS     Extra C flags
#
# Pre-requisite: build & install the library first
#   cd ../src && make clean all install ARCH=$(ARCH)

OUTPUT_FILE  = enc_tool.exe
ARCH        ?= Linux3.10.0_ICC

SRCS         = enc_tool.c
OBJS         = $(SRCS:.c=.o)

CC          ?= gcc

OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS   := $(shell pkg-config --libs   openssl 2>/dev/null)
ifeq ($(OPENSSL_LIBS),)
OPENSSL_LIBS   := -lcrypto
endif

CFLAGS      ?= -O2 -g -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
CFLAGS      += -I../include $(OPENSSL_CFLAGS)

LDFLAGS     ?= -L../lib/$(ARCH)
LIBS         = -lUEnc $(OPENSSL_LIBS)

.PHONY: all clean libdep

# Order-only deps: the library/header must exist before we compile, but
# don't trigger a rebuild on every mtime bump.
all: | libdep
all: $(OUTPUT_FILE)

libdep:
	@if [ ! -f ../lib/$(ARCH)/libUEnc.a ] || [ ! -f ../include/UEnc.h ]; then \
		echo "==> libUEnc not installed - building from ../src"; \
		$(MAKE) -C ../src ARCH=$(ARCH) all install; \
	fi

$(OUTPUT_FILE): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

$(OBJS): | libdep

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(OUTPUT_FILE)
