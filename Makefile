include Makefile.common

SRCDIR = $(ROOT)/src
LIBDIR = $(ROOT)/lib

CFLAGS += -I$(SRCDIR)/src
CFLAGS += -DSTATS
CFLAGS += -DROLL_OVER_CLOCK
CFLAGS += -DTLS

# Choose write-back or write-through version
VER = wb
# VER = wt

LIBS = $(LIBDIR)/libtstm.a
TLIBS = $(LIBDIR)/libtanger-stm.a $(LIBDIR)/libtanger-stm.bc
TESTS = test/intset test/regression

.PHONY:	all clean tanger test

all:	$(LIBS)

tanger:	$(TLIBS)

%.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.bc:	%.c
	$(LLVMGCC) $(CFLAGS) -emit-llvm -c -o $@ $<

$(LIBDIR)/libtstm.a:	$(SRCDIR)/tinySTM-$(VER).o $(SRCDIR)/memory.o $(SRCDIR)/wrappers.o
	$(AR) cru $@ $^

$(LIBDIR)/libtanger-stm.a:	$(SRCDIR)/tinySTM-$(VER).o $(SRCDIR)/wrappers.o $(SRCDIR)/tanger.o
	$(AR) cru $@ $^

$(LIBDIR)/libtanger-stm.bc:	$(SRCDIR)/tinySTM-$(VER).bc $(SRCDIR)/wrappers.bc $(SRCDIR)/tanger.bc
	$(LLVMLD) -link-as-library -o $@ $^

test:	$(LIBS)
	$(MAKE) -C test

clean:
	rm -f $(LIBS) $(TLIBS) $(SRCDIR)/*.o $(SRCDIR)/*.bc

realclean:	clean
	TARGET=clean $(MAKE) -C test
