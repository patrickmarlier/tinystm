ROOT ?= ../..

include $(ROOT)/Makefile.common

BINS = intset-hs intset-ll intset-rb intset-sl

# TODO Change $(shell pwd) since it is not portable (find a way to find absolute path)
# TODO this is useless now. Should be in abi/*/Makefile. check before to remove
ifdef TM_ABI
  CFLAGS += -DTM_ABI
  LDFLAGS += -Wl,-rpath=$(shell pwd)/../../abi/gcc
  LDFLAGS += -L$(ROOT)/abi/gcc 
  LDFLAGS += -litm
else ifdef TM_GCC
  CFLAGS += -DTM_GCC -fgnu-tm
  LDFLAGS += -Wl,-rpath=$(shell pwd)/../../abi/gcc
  LDFLAGS += -L$(ROOT)/abi/gcc 
  LDFLAGS += -litm
else ifdef TM_INTEL
  CFLAGS += -DTM_INTEL -Qtm_enabled
  LDFLAGS += -L$(ROOT)/abi/intel
else ifdef TM_DTMC
# FIXME DTMC need -emit-llvm + llvm-gcc + tmlink...
  CFLAGS += -DTM_DTMC -fgnu-tm
  LDFLAGS += -L$(ROOT)/abi/dtmc
endif

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

# FIXME in case of ABI $(TMLIB) must be replaced to abi/...
$(BINS):	%:	%.o $(TMLIB)
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
