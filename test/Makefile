.PHONY:	all

TESTS = bank intset regression

.PHONY:	all $(TESTS)

all:	$(TESTS)

$(TESTS):
	$(MAKE) -C $@ $(TARGET)
