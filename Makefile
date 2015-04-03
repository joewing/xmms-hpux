
# Some path info.
XMMS_PREFIX = /usr/local
GLIB_PREFIX = /usr/local

# Set this to point to libgcc.a
LIB_GCC = /usr/local/lib/gcc/hppa2.0w-hp-hpux11.11/4.1.0/libgcc.a

# The compiler and linker to use.
CC = gcc
LD = /usr/ccs/bin/ld

# Compiler flags.
GLIB_CFLAGS = -I$(GLIB_PREFIX)/include/glib-1.2 -I$(GLIB_PREFIX)/lib/glib/include
XMMS_CFLAGS = -I$(XMMS_PREFIX)/include/xmms $(GLIB_CFLAGS)

CFLAGS = -O2 -Wall $(XMMS_CFLAGS) -D_REENTRANT -D_THREAD_SAFE -DPIC

# Link flags.
LDFLAGS = -lm -lc $(LIB_GCC)

OBJECTS = hpux.lo mixer.lo audio.lo convert.lo
OUTPUT = libhpux.so

.SUFFIXES: .c .lo .so .h

all: $(OUTPUT)

install: $(OUTPUT)
	cp $(OUTPUT) $(XMMS_PREFIX)/lib/xmms/Output/$(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(LD) -b +h $(OUTPUT) +b $(XMMS_PREFIX)/lib/xmms/Output -o $(OUTPUT) $(OBJECTS) $(LDFLAGS)

.c.lo: $*.c
	$(CC) $(CFLAGS) -MD -MP -MF $*.Tpo -c -MT $*.lo -o $*.lo $*.c -fPIC

clean:
	rm -f *.lo *.Tpo $(OUTPUT)

