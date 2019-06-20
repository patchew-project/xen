ifneq ($(filter y,$(CONFIG_XEN_GUEST) $(CONFIG_XEN_NESTED) $(CONFIG_PVH_GUEST)),)
obj-y += hypercall_page.o
endif
obj-y += xen.o
obj-$(CONFIG_XEN_GUEST) += xen-guest.o
obj-$(CONFIG_XEN_NESTED) += xen-nested.o

obj-bin-$(CONFIG_PVH_GUEST) += pvh-boot.init.o
