ROOT = ../..

include $(ROOT)/Makefile.common

BINS = types

.PHONY:	all clean

all:	$(BINS)

%.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINS):	%:	%.o $(TMLIB)
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
