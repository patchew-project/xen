ifeq ($(CONFIG_SHADOW_PAGING),y)
obj-y += common.o guest_2.o guest_3.o guest_4.o
obj-$(CONFIG_HVM) += hvm.o
obj-$(CONFIG_PV) += pv.o
else
obj-y += none.o
endif

guest_%.o guest_%.i guest_%.s: CFLAGS-y += -DGUEST_PAGING_LEVELS=$*

guest_%.o: multi.c FORCE
	$(call if_changed_rule,cc_o_c)

guest_%.i: multi.c FORCE
	$(call if_changed,cpp_i_c)

guest_%.s: multi.c FORCE
	$(call if_changed,cc_s_c)
