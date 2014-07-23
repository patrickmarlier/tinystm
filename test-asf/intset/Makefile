ROOT ?= ../..

include $(ROOT)/Makefile.common

BINS = intset-hs intset-ll intset-rb intset-sl

# HYBRID_ASF define is used for stm.h
# TODO find a solution to force building HybridTM
# TODO this version doesn't yet support ABI version (to be tested)
CFLAGS += -DHYBRID_ASF
# Static is required in most case since the GLIBC can be different
LDFLAGS += -static

.PHONY:	all clean

all:	$(BINS)

intset-hs.o:	intset.c
	$(CC) $(CFLAGS) $(DEFINES) -DUSE_HASHSET -c -o $@ $<

intset-ll.o:	intset.c
	$(CC) $(CFLAGS) $(DEFINES) -DUSE_LINKEDLIST -c -o $@ $<

intset-rb.o:	intset.c
	$(CC) $(CFLAGS) $(DEFINES) -DUSE_RBTREE -c -o $@ $<

intset-sl.o:	intset.c
	$(CC) $(CFLAGS) $(DEFINES) -DUSE_SKIPLIST -c -o $@ $<

$(BINS):	%:	%.o $(TMLIB)
	$(LD) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
