ROOT = ../..

include $(ROOT)/Makefile.common

BINS = intset intset-rbtree

.PHONY:	all clean

all:	$(BINS)

%.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINS):	%:	%.o
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
