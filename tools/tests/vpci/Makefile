XEN_ROOT=$(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

TARGET := test_vpci

.PHONY: all
all: $(TARGET)

.PHONY: run
run: $(TARGET)
	./$(TARGET)

$(TARGET): vpci.c vpci.h list.h main.c emul.h
	$(HOSTCC) -g -o $@ vpci.c main.c

.PHONY: clean
clean:
	rm -rf $(TARGET) *.o *~ vpci.h vpci.c list.h

.PHONY: distclean
distclean: clean

.PHONY: install
install:

vpci.c: $(XEN_ROOT)/xen/drivers/vpci/vpci.c
	# Remove includes and add the test harness header
	sed -e '/#include/d' -e '1s/^/#include "emul.h"/' \
	    -e 's/^static uint32_t read/uint32_t vpci_read/' \
	    -e 's/^static void write/void vpci_write/' <$< >$@


list.h: $(XEN_ROOT)/xen/include/xen/list.h
vpci.h: $(XEN_ROOT)/xen/include/xen/vpci.h
list.h vpci.h:
	sed -e '/#include/d' <$< >$@
