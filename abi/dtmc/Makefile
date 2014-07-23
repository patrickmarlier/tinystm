# Path to tinySTM
ROOT = ../..

.PHONY:	dtmc all clean check

# ROOT must be defined to include Makefile.common
include $(ROOT)/abi/Makefile.common

##############################################################################
## DTMC
##############################################################################
LLVMCC ?= llvm-gcc
LLVMLD ?= llvm-ld

CFLAGS += -DTM_DTMC 

#FIXME TEST with NO_TLS! many problems with stack variable, must use get_stack()!

all: 	dtmc

# TODO find a way to avoid compiling not
#LLVMCC_OK = $(shell $(LLVMCC) ; if [ $$? -neq 0 ] ; then echo "no differences" ; fi)
#@$(echo) "TEST"
#if [ $$? -eq 0 ] ; then echo "no differences" ; fi
#$(shell echo $(LLVMCC_OK))

# TODO DTMC must have THREAD_WRAPPER until I manage in tanger_save_stack the thread_init
DEF_ABI_TANGER = $(subst -UPTHREAD_WRAPPER,,$(DEF_ABI)) -DPTHREAD_WRAPPER

# -DNO_STACK_CHECK => shouldn't be used
# TODO Add possibility to create 32 bits lib => llvm-ld needs -m32?
# TODO find a good way to add dependencies abi.c tanger.c arch.S and also the lib (otherwise it will not be built)
%.bc:   ../%.c
	$(LLVMCC) $(CFLAGS) $(DEF_ABI_TANGER) -emit-llvm -c -o $@ $<

# TODO hidden dependency tanger.c => how to handle it?
dtmc: 	abi.bc ../arch.S
#	$(LLVMCC) $(CFLAGS) $(DEF_ABI_TANGER) -emit-llvm -c -o abi.bc ../abi.c
	$(LLVMLD) -link-as-library -o libtanger-stm.bc abi.bc
	$(CC) $(CFLAGS) -o arch.o -c ../arch.S
	$(AR) cru libtanger-stm.a arch.o

##############################################################################

# FIXME DTMC needs specific compilation...
TANGERDIR       ?= YOU_DID_NOT_SET_TANGERDIR
LLVMBUILD       ?= $(shell llvm-config --build-mode)
ifeq ($(shell uname),Darwin)
  LIBTYPE       ?= dylib
else
  LIBTYPE       ?= so
endif
#STMLIBDIR       ?= $(TANGERDIR)/$(LLVMBUILD)/lib/
STMLIBDIR       ?= .
STMSUPPORTDIR   ?= $(TANGERDIR)/$(LLVMBUILD)/lib/
TANGERPASS      ?= $(TANGERDIR)/$(LLVMBUILD)/lib/libtanger.$(LIBTYPE)
TMLINK          ?= $(TANGERDIR)/$(LLVMBUILD)/bin/tmlink
TMLINKFLAGS     ?= -tanger=$(TANGERPASS) -stmsupport=$(STMSUPPORTDIR) -stmlib=$(STMLIBDIR)
LLVMCCFLAGS     ?= --gnu-tm -emit-llvm

TESTCC          ?= $(LLVMCC)
TESTCFLAGS      += $(LLVMCCFLAGS) -DTM_DTMC

TESTLD          ?= $(TMLINK)
TESTLDFLAGS     := $(TMLINKFLAGS) $(TESTLDFLAGS) -ldl 

clean: 	intset-clean
	rm -f *.bc *.o libtanger-stm.bc libtanger-stm.a

