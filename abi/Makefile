
.PHONY:	all gcc dtmc intel clean test check

all:	gcc dtmc intel 

gcc:
	$(MAKE) -C gcc

gcc%:
	$(MAKE) -C gcc $(subst -, , $(subst gcc,,$@))

dtmc:
	$(MAKE) -C dtmc

dtmc%:
	$(MAKE) -C dtmc $(subst -, , $(subst dtmc,,$@))

intel:
	$(MAKE) -C intel

intel%:
	$(MAKE) -C intel $(subst -, , $(subst intel,,$@))

clean:
	$(MAKE) -C test clean
	$(MAKE) -C gcc clean
	$(MAKE) -C dtmc clean
	$(MAKE) -C intel clean

# simple test and check that use explicit calls to gcc libitm library
test:
	$(MAKE) -C test test

check:
	$(MAKE) -C test check

# test-all and check-all need TM compilers (INTEL/DTMC/GCC) in the path 
test-all:
	$(MAKE) -C gcc test
	$(MAKE) -C dtmc test
	$(MAKE) -C intel test

check-all:
	$(MAKE) -C gcc check
	$(MAKE) -C dtmc check
	$(MAKE) -C intel check

