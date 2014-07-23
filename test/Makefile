.PHONY:	all

TESTS = intset regression

.PHONY:	all $(TESTS)

all:	$(TESTS)

$(TESTS):
	$(MAKE) -C $@ $(TARGET)
