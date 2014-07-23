include Makefile.common

CFLAGS += -I$(SRCDIR)

DEFINES += -DINTERNAL_STATS
DEFINES += -DROLL_OVER_CLOCK
DEFINES += -DTLS
# DEFINES += -DCLOCK_IN_CACHE_LINE
# DEFINES += -DNO_DUPLICATES_IN_RW_SETS
# DEFINES += -DWAIT_YIELD
# DEFINES += -DUSE_BLOOM_FILTER

# DEFINES += -DDEBUG -DDEBUG2

# Pick one design
DEFINES += -DDESIGN=WRITE_BACK_ETL
# DEFINES += -DDESIGN=WRITE_BACK_CTL
# DEFINES += -DDESIGN=WRITE_THROUGH

# Pick one contention manager (CM)
DEFINES += -DCM=CM_SUICIDE
# DEFINES += -DCM=CM_DELAY
# DEFINES += -DCM=CM_BACKOFF
# DEFINES += -DCM=CM_PRIORITY

CFLAGS += $(DEFINES)

MODULES := $(patsubst %.c,%.o,$(wildcard $(SRCDIR)/mod_*.c))

.PHONY:	all test clean realclean

all:	$(TMLIB)

%.o:	%.c
	$(CC) $(CFLAGS) -DCOMPILE_FLAGS="$(CFLAGS)" -c -o $@ $<

%.o.c:	%.c
	$(CC) $(CFLAGS) -DCOMPILE_FLAGS="$(CFLAGS)" -E -Wp,-CC,-P -o $@ $<

$(TMLIB):	$(SRCDIR)/$(TM).o $(SRCDIR)/wrappers.o $(MODULES)
	$(AR) cru $@ $^

test:	$(TMLIB)
	$(MAKE) -C test

clean:
	rm -f $(TMLIB) $(SRCDIR)/*.o $(SRCDIR)/*.bc
	TARGET=clean $(MAKE) -C test
