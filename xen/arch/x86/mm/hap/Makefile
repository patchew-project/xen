obj-y += hap.o
obj-y += guest_walk_2level.o
obj-y += guest_walk_3level.o
obj-y += guest_walk_4level.o
obj-y += nested_hap.o
obj-y += nested_ept.o

$(foreach sfx,o i s,$(obj)/guest_walk_%level.$(sfx)): ccflags-y = -DGUEST_PAGING_LEVELS=$*

$(obj)/guest_walk_%level.o: $(src)/guest_walk.c FORCE
	$(call if_changed_dep,cc_o_c)
$(obj)/guest_walk_%level.i: $(src)/guest_walk.c FORCE
	$(call if_changed_dep,cpp_i_c)
$(obj)/guest_walk_%level.s: $(src)/guest_walk.c FORCE
	$(call if_changed_dep,cc_s_c)
