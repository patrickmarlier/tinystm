ROOT = ..

include $(ROOT)/Makefile.common

# LLVM
LLVMCC ?= llvm-gcc
LLVMLD ?= llvm-ld

# Path to Tanger
TANGER_HOME ?= /usr/local

CFLAGS += -I$(TANGER_HOME)/include

########################################################################
# Compilation options.  Note that the definitions from the main makefile
# are not used here, so one can use different options for Tanger.
########################################################################

# DEFINES += -DDESIGN=WRITE_BACK_ETL
# DEFINES += -DDESIGN=WRITE_BACK_CTL
DEFINES += -DDESIGN=WRITE_THROUGH

DEFINES += -DCM=CM_SUICIDE
# DEFINES += -DCM=CM_DELAY
# DEFINES += -DCM=CM_BACKOFF
# DEFINES += -DCM=CM_PRIORITY

DEFINES += -DROLLOVER_CLOCK
DEFINES += -DCLOCK_IN_CACHE_LINE
DEFINES += -DEPOCH_GC
# DEFINES += -DREAD_LOCKED_DATA

DEFINES += -DLOCK_SHIFT_EXTRA=0

# DEFINES += -DNO_STACK_CHECK
# DEFINES += -DTANGER_STATS

########################################################################
# Do not modify anything below this point!
########################################################################

# Use explicit parameters for Tanger
DEFINES += -DEXPLICIT_TX_PARAMETER

TANGERLIB = $(LIBDIR)/libtanger-stm

ifneq (,$(findstring -DEPOCH_GC,$(DEFINES)))
 GC := gc.c
else
 GC :=
endif

MODULES := mod_mem.c

.PHONY:	all clean

all:	$(TANGERLIB).bc $(TANGERLIB).a

%.bc:	%.c
	$(LLVMCC) $(CFLAGS) $(DEFINES) -emit-llvm -c -o $@ $<

%.bc:	$(SRCDIR)/%.c
	$(LLVMCC) $(CFLAGS) $(DEFINES) -emit-llvm -c -o $@ $<

%.o:	%.c
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

%.o:	$(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

$(TANGERLIB).bc:	$(patsubst %.c,%.bc,$(TM).c wrappers.c $(GC) $(MODULES) tanger.c)
	$(LLVMLD) -link-as-library -o $@ $^

$(TANGERLIB).a:	$(patsubst %.c,%.o,$(TM).c wrappers.c $(GC) $(MODULES) tanger.c)
	$(AR) cru $@ $^

clean:
	rm -f $(TANGERLIB).bc $(TANGERLIB).a *.bc *.o
