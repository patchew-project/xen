ifneq ($(CONFIG_COMPAT),)

compat-arch-$(CONFIG_X86) := x86_32

headers-y := \
    compat/argo.h \
    compat/callback.h \
    compat/elfnote.h \
    compat/event_channel.h \
    compat/features.h \
    compat/grant_table.h \
    compat/kexec.h \
    compat/memory.h \
    compat/nmi.h \
    compat/physdev.h \
    compat/platform.h \
    compat/sched.h \
    compat/trace.h \
    compat/vcpu.h \
    compat/version.h \
    compat/xen.h \
    compat/xenoprof.h
headers-$(CONFIG_X86)     += compat/arch-x86/pmu.h
headers-$(CONFIG_X86)     += compat/arch-x86/xen-mca.h
headers-$(CONFIG_X86)     += compat/arch-x86/xen.h
headers-$(CONFIG_X86)     += compat/arch-x86/xen-$(compat-arch-y).h
headers-$(CONFIG_X86)     += compat/hvm/dm_op.h
headers-$(CONFIG_X86)     += compat/hvm/hvm_op.h
headers-$(CONFIG_X86)     += compat/hvm/hvm_vcpu.h
headers-y                 += compat/arch-$(compat-arch-y).h compat/pmu.h compat/xlat.h
headers-$(CONFIG_XSM_FLASK) += compat/xsm/flask_op.h

cppflags-y                := -include public/xen-compat.h -DXEN_GENERATING_COMPAT_HEADERS
cppflags-$(CONFIG_X86)    += -m32

# 8-byte types are 4-byte aligned on x86_32 ...
ifeq ($(clang),y)
prefix-$(CONFIG_X86)      := \#pragma pack(push, 4)
suffix-$(CONFIG_X86)      := \#pragma pack(pop)
else
prefix-$(CONFIG_X86)      := \#pragma pack(4)
suffix-$(CONFIG_X86)      := \#pragma pack()
endif

endif

public-$(CONFIG_X86) := $(wildcard $(srctree)/$(src)/public/arch-x86/*.h $(srctree)/$(src)/public/arch-x86/*/*.h)
public-$(CONFIG_ARM) := $(wildcard $(srctree)/$(src)/public/arch-arm/*.h $(srctree)/$(src)/public/arch-arm/*/*.h)

all: $(addprefix $(obj)/,$(headers-y))

$(obj)/compat/%.h: $(obj)/compat/%.i $(srctree)/tools/compat-build-header.py FORCE
	$(call if_changed,compat_header)

quiet_cmd_compat_header = COMPAT-H $@
define cmd_compat_header
	set -e; id=_$$(echo $(patsubst $(obj)/%,%,$(@)) | tr '[:lower:]-/.' '[:upper:]___'); \
	echo "#ifndef $$id" >$@.new; \
	echo "#define $$id" >>$@.new; \
	echo "#include <xen/compat.h>" >>$@.new; \
	$(if $(filter-out $(obj)/compat/arch-%.h,$@),echo "#include <$(patsubst $(obj)/compat/%,public/%,$@)>" >>$@.new;) \
	$(if $(prefix-y),echo "$(prefix-y)" >>$@.new;) \
	grep -v '^# [0-9]' $< | \
	(cd $(srctree)/$(src); $(PYTHON) $(abs_srctree)/tools/compat-build-header.py) | uniq >>$@.new; \
	$(if $(suffix-y),echo "$(suffix-y)" >>$@.new;) \
	echo "#endif /* $$id */" >>$@.new; \
	mv -f $@.new $@
endef

$(obj)/compat/%.i: $(obj)/compat/%.c FORCE
	$(call if_changed,compat_i_c)

quiet_cmd_compat_i_c = CPP     $@
      cmd_compat_i_c = $(CPP) $(filter-out -Wa$(comma)% -include %/include/xen/config.h,$(KBUILD_CFLAGS)) $(cppflags-y) -o $@ $<

$(obj)/compat/%.c: $(src)/public/%.h $(src)/xlat.lst $(srctree)/tools/compat-build-source.py FORCE
	$(call if_changed,compat_source)

quiet_cmd_compat_source = COMPAT-C $@
define cmd_compat_source
	set -e; \
	mkdir -p $(@D); \
	grep -v 'DEFINE_XEN_GUEST_HANDLE(long)' $< | \
	(cd $(srctree)/$(src); $(PYTHON) $(abs_srctree)/tools/compat-build-source.py) >$@.new; \
	mv -f $@.new $@
endef

$(obj)/compat/.xlat/%.h: $(obj)/compat/%.h $(obj)/compat/.xlat/%.lst $(srctree)/tools/get-fields.sh FORCE
	$(call if_changed,sub_xlat_h)

quiet_cmd_sub_xlat_h = SUBXLAT $@
define cmd_sub_xlat_h
	set -e; \
	while read what name; do \
		$(SHELL) $(srctree)/tools/get-fields.sh "$$what" compat_$$name $< || exit $$?; \
	done <$(patsubst $(obj)/compat/%,$(obj)/compat/.xlat/%,$(basename $<)).lst >$@.new; \
	mv -f $@.new $@
endef

$(obj)/compat/.xlat/%.lst: $(src)/xlat.lst $(src)/Makefile
	$(call filechk,xlat_lst)

define filechk_xlat_lst
	grep -v '^[[:blank:]]*#' $< | sed -ne 's,@arch@,$(compat-arch-y),g' -re 's,[[:blank:]]+$*\.h[[:blank:]]*$$,,p'
endef

xlat-y := $(shell sed -ne 's,@arch@,$(compat-arch-y),g' -re 's,^[?!][[:blank:]]+[^[:blank:]]+[[:blank:]]+,,p' $(srctree)/$(src)/xlat.lst | uniq)
xlat-y := $(filter $(patsubst compat/%,%,$(headers-y)),$(xlat-y))

$(obj)/compat/xlat.h: $(addprefix $(obj)/compat/.xlat/,$(xlat-y))
	$(call if_changed,xlat_h)

quiet_cmd_xlat_h = UPD $@
define cmd_xlat_h
	set -e; \
	cat $(filter %.h,$^) >$@.new; \
	mv -f $@.new $@
endef

ifeq ($(XEN_TARGET_ARCH),$(XEN_COMPILE_ARCH))

all: $(obj)/headers.chk $(obj)/headers99.chk $(obj)/headers++.chk

PUBLIC_HEADERS := $(filter-out $(srctree)/$(src)/public/arch-% $(srctree)/$(src)/public/dom0_ops.h, $(wildcard $(srctree)/$(src)/public/*.h $(srctree)/$(src)/public/*/*.h) $(public-y))

PUBLIC_C99_HEADERS := $(srctree)/$(src)/public/io/9pfs.h $(srctree)/$(src)/public/io/pvcalls.h
PUBLIC_ANSI_HEADERS := $(filter-out $(srctree)/$(src)/public/%ctl.h $(srctree)/$(src)/public/xsm/% $(srctree)/$(src)/public/%hvm/save.h $(PUBLIC_C99_HEADERS), $(PUBLIC_HEADERS))

# XXX for in tree build
$(src)/public/io/9pfs.h-prereq := string
$(src)/public/io/pvcalls.h-prereq := string
# XXX for out of tree build
$(srctree)/$(src)/public/io/9pfs.h-prereq := string
$(srctree)/$(src)/public/io/pvcalls.h-prereq := string

$(obj)/headers.chk: $(PUBLIC_ANSI_HEADERS) $(src)/Makefile
	for i in $(filter %.h,$^); do \
	    $(CC) -x c -ansi -Wall -Werror -include stdint.h \
	          -S -o /dev/null $$i || exit 1; \
	    echo $$i; \
	done >$@.new
	mv $@.new $@

$(obj)/headers99.chk: $(PUBLIC_C99_HEADERS) $(src)/Makefile
	rm -f $@.new
	$(foreach i, $(filter %.h,$^),                                        \
	    echo "#include "\"$(i)\"                                          \
	    | $(CC) -x c -std=c99 -Wall -Werror                               \
	      -include stdint.h $(foreach j, $($(i)-prereq), -include $(j).h) \
	      -S -o /dev/null -                                               \
	    || exit $$?; echo $(i) >> $@.new;)
	mv $@.new $@

$(obj)/headers++.chk: $(PUBLIC_HEADERS) $(src)/Makefile
	rm -f $@.new
	if ! $(CXX) -v >/dev/null 2>&1; then                                  \
	    touch $@.new;                                                     \
	    exit 0;                                                           \
	fi;                                                                   \
	$(foreach i, $(filter %.h,$^),                                        \
	    echo "#include "\"$(i)\"                                          \
	    | $(CXX) -x c++ -std=gnu++98 -Wall -Werror -D__XEN_TOOLS__        \
	      -include stdint.h -include $(srctree)/$(src)/public/xen.h       \
	      $(foreach j, $($(i)-prereq), -include c$(j)) -S -o /dev/null -  \
	    || exit $$?; echo $(i) >> $@.new;)
	mv $@.new $@

endif

ifeq ($(ARCH),x86_64)
.PHONY: lib-x86-all
lib-x86-all:
	$(Q)$(MAKE) XEN_ROOT=$(abs_srctree)/.. -C $(srctree)/$(src)/xen/lib/x86 all

all: lib-x86-all
endif

# Update targets to help Kbuild finding which dependency file to load.
targets += $(headers-y)
# Adding intermediate targets
targets += $(foreach h, $(headers-y), \
		$(h:.h=.i) $(h:.h=.c))
# Adding intermediate targets of xlat.h
targets += $(addprefix compat/.xlat/,$(xlat-y))

clean-dirs += compat config generated
clean-files += headers*.chk
clean-files += xen/lib/x86/cpuid-autogen.h

PHONY += all
all:
	@:
