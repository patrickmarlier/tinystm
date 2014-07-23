ROOT = ../..

include $(ROOT)/Makefile.common

BINS = intset-ll intset-rb intset-sl

.PHONY:	all clean

all:	$(BINS)

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
