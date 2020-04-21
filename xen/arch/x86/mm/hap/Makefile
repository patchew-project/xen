obj-y += hap.o
obj-y += guest_walk_2level.o
obj-y += guest_walk_3level.o
obj-y += guest_walk_4level.o
obj-y += nested_hap.o
obj-y += nested_ept.o

guest_walk_%level.o guest_walk_%level.i guest_walk_%level.s: \
    CFLAGS-y += -DGUEST_PAGING_LEVELS=$*

guest_walk_%level.o: guest_walk.c FORCE
	$(call if_changed_rule,cc_o_c)

guest_walk_%level.i: guest_walk.c FORCE
	$(call if_changed,cpp_i_c)

guest_walk_%level.s: guest_walk.c FORCE
	$(call if_changed,cc_s_c)
