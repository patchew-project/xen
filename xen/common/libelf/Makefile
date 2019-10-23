obj-bin-y := libelf.o
nocov-y += libelf.o

SECTIONS := text data $(SPECIAL_DATA_SECTIONS)

ccflags-y += -Wno-pointer-sign
ldflags-y += -r
OBJCOPYFLAGS += $(foreach s,$(SECTIONS),--rename-section .$(s)=.init.$(s))

libelf-temp-objs := libelf-tools.o libelf-loader.o libelf-dominfo.o #libelf-relocate.o
targets += $(libelf-temp-objs) libelf-temp.o

$(obj)/libelf.o: $(obj)/libelf-temp.o FORCE
	$(call if_changed,objcopy)

$(obj)/libelf-temp.o: $(addprefix $(obj)/,$(libelf-temp-objs)) FORCE
	$(call if_changed,ld)
