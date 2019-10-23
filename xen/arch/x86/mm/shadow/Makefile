ifeq ($(CONFIG_SHADOW_PAGING),y)
obj-y += common.o guest_2.o guest_3.o guest_4.o
obj-$(CONFIG_HVM) += hvm.o
obj-$(CONFIG_PV) += pv.o
else
obj-y += none.o
endif

$(foreach sfx,o i s,$(obj)/guest_%.$(sfx)): ccflags-y = -DGUEST_PAGING_LEVELS=$*

$(obj)/guest_%.o: $(src)/multi.c FORCE
	$(call if_changed_dep,cc_o_c)
$(obj)/guest_%.i: $(src)/multi.c FORCE
	$(call if_changed_dep,cpp_i_c)
$(obj)/guest_%.s: $(src)/multi.c FORCE
	$(call if_changed_dep,cc_s_c)
