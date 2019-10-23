include $(srctree)/$(src)/Makefile.libfdt

SECTIONS := text data $(SPECIAL_DATA_SECTIONS)

obj-y += libfdt.o
nocov-y += libfdt.o

ccflags-y += -I$(srctree)/include/xen/libfdt/
ldflags-y += -r
OBJCOPYFLAGS += $(foreach s,$(SECTIONS),--rename-section .$(s)=.init.$(s))

$(obj)/libfdt.o: $(obj)/libfdt-temp.o FORCE
	$(call if_changed,objcopy)

$(obj)/libfdt-temp.o: $(addprefix $(obj)/,$(LIBFDT_OBJS)) FORCE
	$(call if_changed,ld)
