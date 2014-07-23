include Makefile.common

########################################################################
# TinySTM can be configured in many ways.  The main compilation options
# are described below.  To read more easily through the code, you can
# generate a source file stripped from most of the conditional
# preprocessor directives using:
#
#   make src/stm.o.c
#
# For more details on the LSA algorithm and the design of TinySTM, refer
# to:
#
# [DISC-06] Torvald Riegel, Pascal Felber, and Christof Fetzer.  A Lazy
#   Snapshot Algorithm with Eager Validation.  20th International
#   Symposium on Distributed Computing (DISC), 2006.
#
# [PPoPP-08] Pascal Felber, Christof Fetzer, and Torvald Riegel.
#   Dynamic Performance Tuning of Word-Based Software Transactional
#   Memory.  Proceedings of the 13th ACM SIGPLAN Symposium on Principles
#   and Practice of Parallel Programming (PPoPP), 2008.
########################################################################

########################################################################
# Three different designs can be chosen from, which differ in when locks
# are acquired (encounter-time vs. commit-time), and when main memory is
# updated (write-through vs. write-back).
#
# WRITE_BACK_ETL: write-back with encounter-time locking acquires lock
#   when encountering write operations and buffers updates (they are
#   committed to main memory at commit time).
#
# WRITE_BACK_CTL: write-back with commit-time locking delays acquisition
#   of lock until commit time and buffers updates.
#
# WRITE_THROUGH: write-through (encounter-time locking) directly updates
#   memory and keeps an undo log for possible rollback.
#
# Refer to [PPoPP-08] for more details.
########################################################################

# DEFINES += -DDESIGN=WRITE_BACK_ETL
# DEFINES += -DDESIGN=WRITE_BACK_CTL
DEFINES += -DDESIGN=WRITE_THROUGH

########################################################################
# Several contention management strategies are available:
#
# CM_SUICIDE: immediately abort the transaction that detects the
#   conflict.
#
# CM_DELAY: like CM_SUICIDE but wait until the contended lock that
#   caused the abort (if any) has been released before restarting the
#   transaction.  The intuition is that the transaction will likely try
#   again to acquire the same lock and might fail once more if it has
#   not been released.  In addition, this increases the chances that the
#   transaction can succeed with no interruption upon retry, which
#   improves execution time on the processor.
#
# CM_DELAY: like CM_SUICIDE but wait for a random delay before
#   restarting the transaction.  The delay duration is chosen uniformly
#   at random from a range whose size increases exponentially with every
#   restart.
#
# CM_PRIORITY: cooperative priority-based contention manager that avoids
#   livelocks.  It only works with the ETL-based design (WRITE_BACK_ETL
#   or WRITE_THROUGH).  The principle is to give preference to
#   transactions that have already aborted many times.  Therefore, a
#   priority is associated to each transaction and it increases with the
#   number of retries.
# 
#   A transaction that tries to acquire a lock can "reserve" it if it is
#   currently owned by another transaction with lower priority.  If the
#   latter is blocked waiting for another lock, it will detect that the
#   former is waiting for the lock and will abort.  As with CM_DELAY,
#   before retrying after failing to acquire some lock, we wait until
#   the lock we were waiting for is released.
#
#   If a transaction fails because of a read-write conflict (detected
#   upon validation at commit time), we do not increase the priority.
#   It such a failure occurs sufficiently enough (e.g., three times in a
#   row, can be parametrized), we switch to visible reads.
#
#   When using visible reads, each read is implemented as a write and we
#   do not allow multiple readers.  The reasoning is that (1) visible
#   reads are expected to be used rarely, (2) supporting multiple
#   readers is complex and has non-negligible overhead, especially if
#   fairness must be guaranteed, e.g., to avoid writer starvation, and
#   (3) having a single reader makes lock upgrade trivial.
#
#   To implement cooperative contention management, we associate a
#   priority to each transaction.  The priority is used to avoid
#   deadlocks and to decide which transaction can proceed or must abort
#   upon conflict.  Priorities can vary between 0 and MAX_PRIORITY.  By
#   default we use 3 bits, i.e., MAX_PRIORITY=7, and we use the number
#   of retries of a transaction to specify its priority.  The priority
#   of a transaction is encoded in the locks (when the lock bit is set).
#   If the number of concurrent transactions is higher than
#   MAX_PRIORITY+1, the properties of the CM (bound on the number of
#   retries) might not hold.
#
#   The priority contention manager can be activated only after a
#   configurable number of retries.  Until then, CM_SUICIDE is used.
########################################################################

# Pick one contention manager (CM)
DEFINES += -DCM=CM_SUICIDE
# DEFINES += -DCM=CM_DELAY
# DEFINES += -DCM=CM_BACKOFF
# DEFINES += -DCM=CM_PRIORITY

########################################################################
# Maintain detailed internal statistics.  Statistics are stored in
# thread locals and do not add much overhead, so do not expect much gain
# from disabling them.
########################################################################

DEFINES += -DINTERNAL_STATS
# DEFINES += -UINTERNAL_STATS

########################################################################
# Roll over clock when it reaches its maximum value.  Clock rollover can
# be safely disabled on 64 bits to save a few cycles, but it is
# necessary on 32 bits if the application executes more than 2^28
# (write-through) or 2^31 (write-back) transactions.
########################################################################

DEFINES += -DROLLOVER_CLOCK
# DEFINES += -UROLLOVER_CLOCK

########################################################################
# Ensure that the global clock does not share the same cache line than
# some other variable of the program.  This should be normally enabled.
########################################################################

DEFINES += -DCLOCK_IN_CACHE_LINE
# DEFINES += -UCLOCK_IN_CACHE_LINE

########################################################################
# Prevent duplicate entries in read/write sets when accessing the same
# address multiple times.  Enabling this option may reduce performance
# so leave it disabled unless transactions repeatedly read or write the
# same address.
########################################################################

# DEFINES += -DNO_DUPLICATES_IN_RW_SETS
DEFINES += -UNO_DUPLICATES_IN_RW_SETS

########################################################################
# Yield the processor when waiting for a contended lock to be released.
# This only applies to the CM_WAIT and CM_PRIORITY contention managers.
########################################################################

# DEFINES += -DWAIT_YIELD
DEFINES += -UWAIT_YIELD

########################################################################
# Use a (degenerate) bloom filter for quickly checking in the write set
# whether an address has previously been written.  This approach is
# directly inspired by TL2.  It only applies to the WRITE_BACK_CTL
# design.
########################################################################

# DEFINES += -DUSE_BLOOM_FILTER
DEFINES += -UUSE_BLOOM_FILTER

########################################################################
# Use an epoch-based memory allocator and garbage collector to ensure
# that accesses to the dynamic memory allocated by a transaction from
# another transaction are valid.  There is a slight overhead from
# enabling this feature.
########################################################################

DEFINES += -DEPOCH_GC
# DEFINES += -UEPOCH_GC

########################################################################
# Keep track of conflicts between transactions and notifies the
# application (using a callback), passing the identity of the two
# conflicting transaction and the associated threads.  This feature
# requires EPOCH_GC.
########################################################################

# DEFINES += -DCONFLICT_TRACKING
DEFINES += -UCONFLICT_TRACKING

########################################################################
# Allow transactions to read the previous version of locked memory
# locations, as in the original LSA algorithm (see [DISC-06]).  This is
# achieved by peeking into the write set of the transaction that owns
# the lock.  There is a small overhead with non-contended workloads but
# it may significantly reduce the abort rate, especially with
# transactions that read much data.  This feature only works with the
# WRITE_BACK_ETL design and requires EPOCH_GC.
########################################################################

# DEFINES += -DREAD_LOCKED_DATA
DEFINES += -UREAD_LOCKED_DATA

########################################################################
# Tweak the hash function that maps addresses to locks so that
# consecutive addresses do not map to consecutive locks.  This can avoid
# cache line invalidations for application that perform sequential
# memory accesses.  The last byte of the lock index is swapped with the
# previous byte.
########################################################################

# DEFINES += -DLOCK_IDX_SWAP
DEFINES += -ULOCK_IDX_SWAP

########################################################################
# Output many (DEBUG) or even mode (DEBUG2) debugging messages.
########################################################################

# DEFINES += -DDEBUG
DEFINES += -UDEBUG
# DEFINES += -DDEBUG2
DEFINES += -UDEBUG2

########################################################################
# Various default values can also be overridden:
#
# RW_SET_SIZE (default=4096): initial size of the read and write
#   sets.  These sets will grow dynamically when they become full.
#
# LOCK_ARRAY_LOG_SIZE (default=20): number of bits used for indexes in
#   the lock array.  The size of the array will be 2 to the power of
#   LOCK_ARRAY_LOG_SIZE.
#
# LOCK_SHIFT_EXTRA (default=2): additional shifts to apply to the
#   address when determining its index in the lock array.  This controls
#   how many consecutive memory words will be covered by the same lock
#   (2 to the power of LOCK_SHIFT_EXTRA).  Higher values will increase
#   false sharing but reduce the number of CASes necessary to acquire
#   locks and may avoid cache line invalidations on some workloads.  As
#   shown in [PPoPP-08], a value of 2 seems to offer best performance on
#   many benchmarks.
#
# MIN_BACKOFF (default=0x04UL) and MAX_BACKOFF (default=0x80000000UL):
#   minimum and maximum values of the exponential backoff delay.  This
#   parameter is only used with the CM_BACKOFF contention manager.
#
# VR_THRESHOLD_DEFAULT (default=3): number of aborts due to failed
#   validation before switching to visible reads.  A value of 0
#   indicates no limit.  This parameter is only used with the
#   CM_PRIORITY contention manager.  It can also be set using an
#   environment variable of the same name.
#
# CM_THRESHOLD_DEFAULT (default=0): number of executions of the
#   transaction with a CM_SUICIDE contention management strategy before
#   switching to CM_PRIORITY.  This parameter is only used with the
#   CM_PRIORITY contention manager.  It can also be set using an
#   environment variable of the same name.
########################################################################

# DEFINES += -DRW_SET_SIZE=4096
# DEFINES += -DLOCK_ARRAY_LOG_SIZE=20
# DEFINES += -DLOCK_SHIFT_EXTRA=2
# DEFINES += -DMIN_BACKOFF=0x04UL
# DEFINES += -DMAX_BACKOFF=0x80000000UL
# DEFINES += -DVR_THRESHOLD_DEFAULT=3
# DEFINES += -DCM_THRESHOLD_DEFAULT=0

#DEFINES += -DEXPLICIT_TX_PARAMETER

########################################################################
# Do not modify anything below this point!
########################################################################

# Replace textual values by constants for unifdef...
D := $(DEFINES)
D := $(D:WRITE_BACK_ETL=0)
D := $(D:WRITE_BACK_CTL=1)
D := $(D:WRITE_THROUGH=2)
D += -DWRITE_BACK_ETL=0 -DWRITE_BACK_CTL=1 -DWRITE_THROUGH=2
D := $(D:CM_SUICIDE=0)
D := $(D:CM_DELAY=1)
D := $(D:CM_BACKOFF=2)
D := $(D:CM_PRIORITY=3)
D += -DCM_SUICIDE=0 -DCM_DELAY=1 -DCM_BACKOFF=2 -DCM_PRIORITY=3

ifneq (,$(findstring -DEPOCH_GC,$(DEFINES)))
 GC := $(SRCDIR)/gc.o
else
 GC :=
endif

CFLAGS += -I$(SRCDIR)
CFLAGS += $(DEFINES)

MODULES := $(patsubst %.c,%.o,$(wildcard $(SRCDIR)/mod_*.c))

.PHONY:	all doc test tanger clean

all:	$(TMLIB)

%.o:	%.c
	$(CC) $(CFLAGS) -DCOMPILE_FLAGS="$(CFLAGS)" -c -o $@ $<

%.s:	%.c
	$(CC) $(CFLAGS) -DCOMPILE_FLAGS="$(CFLAGS)" -fverbose-asm -S -o $@ $<

%.o.c:	%.c
	$(UNIFDEF) $(D) $< > $@ || true

$(TMLIB):	$(SRCDIR)/$(TM).o $(SRCDIR)/wrappers.o $(GC) $(MODULES)
	$(AR) cru $@ $^

test:	$(TMLIB)
	$(MAKE) -C test

tanger:
	$(MAKE) -C tanger

doc:
	$(DOXYGEN)

clean:
	rm -f $(TMLIB) $(SRCDIR)/*.o
	$(MAKE) -C tanger clean
	TARGET=clean $(MAKE) -C test
