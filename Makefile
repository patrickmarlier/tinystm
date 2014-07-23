CC ?= gcc
AR ?= ar

# Path to LIBATOMIC_OPS
LIBAO_HOME ?= /usr/local

CFLAGS += -Wall -Wno-unused-function -O3 -fno-strict-aliasing -DNDEBUG -D_REENTRANT
CFLAGS += -I$(LIBAO_HOME)/include -Iinclude -Isrc
CFLAGS += -DSTATS
CFLAGS += -DROLL_OVER_CLOCK

LDFLAGS += -L$(LIBAO_HOME)/lib -Llib -latomic_ops -ltstm -lpthread

# Choose write-back or write-through version
# VER = wb
VER = wt

LIBS = lib/libtstm.a
BINS = bin/intset bin/intset-rbtree

all:	$(LIBS) $(BINS)

%.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<

lib/libtstm.a:	src/tinySTM-$(VER).o src/wrappers.o
	$(AR) cru $@ $^

bin/%:	tests/%.o lib/libtstm.a
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(LIBS) $(BINS) src/*.o tests/*.o
