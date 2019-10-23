# Add -Iinclude so cc-option calls can work properly. (The current CFLAGS want
# to include a header which includes <generated/autoconf.h>)
KBUILD_CFLAGS += $(LINUXINCLUDE)
# Prepare CFLAGS to have 32bit build for the deps of head.S
CFLAGS_S_c	:= -m32 -march=i686
CFLAGS_S_c	+= -fno-strict-aliasing -std=gnu99 -Wall -Wstrict-prototypes
CFLAGS_S_c	+= $(call cc-option,-Wdeclaration-after-statement)
CFLAGS_S_c	+= $(call cc-disable-warning,unused-but-set-variable)
CFLAGS_S_c	+= $(call cc-disable-warning,unused-local-typedefs)

EMBEDDED_EXTRA_CFLAGS := -nopie -fno-stack-protector -fno-stack-protector-all
EMBEDDED_EXTRA_CFLAGS += -fno-exceptions
CFLAGS_S_c += $(foreach o,$(EMBEDDED_EXTRA_CFLAGS),$(call cc-option,$(o)))
CFLAGS_S_c += -Werror -fno-asynchronous-unwind-tables -fno-builtin -g0 -msoft-float
CFLAGS_S_c := $(filter-out -flto,$(CFLAGS_S_c))
CFLAGS_S_c += -fpic
LDFLAGS_emulation_OpenBSD := _obsd
LDFLAGS_emulation_FreeBSD := _fbsd
LDFLAGS_S_c := -melf_i386$(LDFLAGS_emulation_$(XEN_OS)) -N -T $(srctree)/$(src)/build32.lds

head-deps := cmdline.S reloc.S
targets += $(foreach o,$(head-deps), \
		$(foreach sfx,o lnk bin S, \
			$(patsubst %.S,%.$(sfx),$(o))))

$(addprefix $(obj)/,$(head-deps:.S=.o)): KBUILD_CFLAGS := $(CFLAGS_S_c)
$(addprefix $(obj)/,$(head-deps:.S=.lnk)): KBUILD_LDFLAGS := $(LDFLAGS_S_c)

$(obj)/head.o: $(addprefix $(obj)/,$(head-deps))
obj-y := head.o

# NB. awk invocation is a portable alternative to 'head -n -1'
quiet_cmd_sed_S_bin = UPD     $@
define cmd_sed_S_bin
	(od -v -t x $< | tr -s ' ' | awk 'NR > 1 {print s} {s=$$0}' | \
	sed 's/ /,0x/g' | sed 's/,0x$$//' | sed 's/^[0-9]*,/ .long /') >$@
endef
$(addprefix $(obj)/,cmdline.S reloc.S): $(obj)/%.S: $(obj)/%.bin FORCE
	$(call if_changed,sed_S_bin)

# Drop .got.plt during conversion to plain binary format.
# Please check build32.lds for more details.
quiet_cmd_check_obj_got_plt = CHECK $@
define cmd_check_obj_got_plt
	set -e; \
	$(OBJDUMP) -h $< | sed -n '/[0-9]/{s,00*,0,g;p;}' | \
		while read idx name sz rest; do \
			case "$$name" in \
			.got.plt) \
				test $$sz != 0c || continue; \
				echo "Error: non-empty $$name: 0x$$sz" >&2; \
				exit $$(expr $$idx + 1);; \
			esac; \
		done
endef
quiet_cmd_check_obj_drop_got_plt = OBJCOPY $@
cmd_check_obj_drop_got_plt = $(cmd_check_obj_got_plt); $(cmd_objcopy)

OBJCOPYFLAGS :=  -O binary -R .got.plt
$(obj)/%.bin: $(obj)/%.lnk FORCE
	$(call if_changed,check_obj_drop_got_plt)

$(obj)/%.lnk: $(obj)/%.o FORCE
	$(call if_changed,ld)
