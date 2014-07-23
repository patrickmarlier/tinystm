ROOT = ../..

include $(ROOT)/Makefile.common

BINS = intset-hs intset-ll intset-rb intset-sl

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
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
