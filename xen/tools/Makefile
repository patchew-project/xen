
include $(XEN_ROOT)/Config.mk

PROGS = symbols bin2c

.PHONY: default
default:
	$(MAKE) $(PROGS)

.PHONY: clean
clean:
	rm -f *.o $(PROGS)

symbols: symbols.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

bin2c: bin2c.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<
