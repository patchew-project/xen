# select defconfig based on actual architecture
KBUILD_DEFCONFIG := $(ARCH)_defconfig

export XEN_IMG_OFFSET := 0x200000

KBUILD_CFLAGS += -I$(srctree)/include
KBUILD_CFLAGS += -I$(srctree)/include/asm-x86/mach-generic
KBUILD_CFLAGS += -I$(srctree)/include/asm-x86/mach-default
KBUILD_CFLAGS += -DXEN_IMG_OFFSET=$(XEN_IMG_OFFSET)

# Prevent floating-point variables from creeping into Xen.
KBUILD_CFLAGS += -msoft-float

KBUILD_CFLAGS += $(foreach o,$(EMBEDDED_EXTRA_CFLAGS),$(call cc-option,$(o)))
KBUILD_CFLAGS += $(call cc-option,-Wnested-externs)
KBUILD_CFLAGS += $(call as-instr,vmcall,-DHAVE_AS_VMX)
KBUILD_CFLAGS += $(call as-instr,crc32 %eax$(comma)%eax,-DHAVE_AS_SSE4_2)
KBUILD_CFLAGS += $(call as-instr,invept (%rax)$(comma)%rax,-DHAVE_AS_EPT)
KBUILD_CFLAGS += $(call as-instr,rdrand %eax,-DHAVE_AS_RDRAND)
KBUILD_CFLAGS += $(call as-instr,rdfsbase %rax,-DHAVE_AS_FSGSBASE)
KBUILD_CFLAGS += $(call as-instr,xsaveopt (%rax),-DHAVE_AS_XSAVEOPT)
KBUILD_CFLAGS += $(call as-instr,rdseed %eax,-DHAVE_AS_RDSEED)
KBUILD_CFLAGS += $(call as-instr,clwb (%rax),-DHAVE_AS_CLWB)
KBUILD_CFLAGS += $(call as-instr,.equ \"x\"$(comma)1,-DHAVE_AS_QUOTED_SYM)
KBUILD_CFLAGS += $(call as-instr,invpcid (%rax)$(comma)%rax,-DHAVE_AS_INVPCID)

# GAS's idea of true is -1.  Clang's idea is 1
KBUILD_CFLAGS += $(call as-instr,\
    .if ((1 > 0) < 0); .error "";.endif,,-DHAVE_AS_NEGATIVE_TRUE)

# Check to see whether the assmbler supports the .nop directive.
KBUILD_CFLAGS += $(call as-instr,\
    .L1: .L2: .nops (.L2 - .L1)$(comma)9,-DHAVE_AS_NOPS_DIRECTIVE)

KBUILD_CFLAGS += -mno-red-zone -fpic -fno-asynchronous-unwind-tables

# Xen doesn't use SSE interally.  If the compiler supports it, also skip the
# SSE setup for variadic function calls.
KBUILD_CFLAGS += -mno-sse $(call cc-option,-mskip-rax-setup)

# -fvisibility=hidden reduces -fpic cost, if it's available
ifeq ($(call cc-option-yn,-fvisibility=hidden),y)
KBUILD_CFLAGS += -DGCC_HAS_VISIBILITY_ATTRIBUTE
endif

# Compile with thunk-extern, indirect-branch-register if avaiable.
ifeq ($(call cc-option-yn,-mindirect-branch-register),y)
KBUILD_CFLAGS += -mindirect-branch=thunk-extern -mindirect-branch-register
KBUILD_CFLAGS += -DCONFIG_INDIRECT_THUNK
KBUILD_CFLAGS += -fno-jump-tables
export CONFIG_INDIRECT_THUNK=y
endif

# If supported by the compiler, reduce stack alignment to 8 bytes. But allow
# this to be overridden elsewhere.
# XXX adding to cflags instead of CFLAGS-stack-boundary
KBUILD_CFLAGS += $(call cc-option,-mpreferred-stack-boundary=3)

# Set up the assembler include path properly for older toolchains.
KBUILD_CFLAGS += -Wa,-I$(srctree)/include

# XXX from xen.git/config/x86_64.mk
LDFLAGS_emulation_OpenBSD := _obsd
LDFLAGS_emulation_FreeBSD := _fbsd
KBUILD_LDFLAGS += -melf_x86_64$(LDFLAGS_emulation_$(XEN_OS))

efi-y := $(shell if [ ! -r $(BASEDIR)/include/xen/compile.h -o \
                      -O $(BASEDIR)/include/xen/compile.h ]; then \
                         echo '$(TARGET).efi'; fi)

ALL_OBJS := $(BASEDIR)/arch/x86/boot/built_in.o $(BASEDIR)/arch/x86/efi/built_in.o $(ALL_OBJS)

ifeq ($(CONFIG_LTO),y)
# Gather all LTO objects together
prelink_lto.o: $(ALL_OBJS)
	$(LD_LTO) -r -o $@ $^

prelink-efi_lto.o: $(ALL_OBJS) efi/runtime.o efi/compat.o
	$(guard) $(LD_LTO) -r -o $@ $(filter-out %/efi/built_in.o,$^)

# Link it with all the binary objects
prelink.o: $(patsubst %/built_in.o,%/built_in_bin.o,$(ALL_OBJS)) prelink_lto.o
	$(LD) $(LDFLAGS) -r -o $@ $^

prelink-efi.o: $(patsubst %/built_in.o,%/built_in_bin.o,$(ALL_OBJS)) prelink-efi_lto.o efi/boot.init.o
	$(guard) $(LD) $(LDFLAGS) -r -o $@ $^
else
prelink-efi.o: $(ALL_OBJS) efi/boot.init.o efi/runtime.o efi/compat.o
	$(guard) $(LD) $(LDFLAGS) -r -o $@ $(filter-out %/efi/built_in.o,$^)
endif

note.o: $(TARGET)-syms
	$(OBJCOPY) -O binary --only-section=.note.gnu.build-id  $(BASEDIR)/xen-syms $@.bin
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		--rename-section=.data=.note.gnu.build-id -S $@.bin $@
	rm -f $@.bin

EFI_LDFLAGS = $(patsubst -m%,-mi386pep,$(LDFLAGS)) --subsystem=10
EFI_LDFLAGS += --image-base=$(1) --stack=0,0 --heap=0,0 --strip-debug
EFI_LDFLAGS += --section-alignment=0x200000 --file-alignment=0x20
EFI_LDFLAGS += --major-image-version=$(XEN_VERSION)
EFI_LDFLAGS += --minor-image-version=$(XEN_SUBVERSION)
EFI_LDFLAGS += --major-os-version=2 --minor-os-version=0
EFI_LDFLAGS += --major-subsystem-version=2 --minor-subsystem-version=0

# Check if the compiler supports the MS ABI.
export XEN_BUILD_EFI := $(shell $(CC) $(filter-out $(CFLAGS-y) .%.d,$(CFLAGS)) -c efi/check.c -o efi/check.o 2>/dev/null && echo y)
# Check if the linker supports PE.
XEN_BUILD_PE := $(if $(XEN_BUILD_EFI),$(shell $(LD) -mi386pep --subsystem=10 -o efi/check.efi efi/check.o 2>/dev/null && echo y))
CFLAGS-$(XEN_BUILD_EFI) += -DXEN_BUILD_EFI

$(TARGET).efi: VIRT_BASE = 0x$(shell $(NM) efi/relocs-dummy.o | sed -n 's, A VIRT_START$$,,p')
$(TARGET).efi: ALT_BASE = 0x$(shell $(NM) efi/relocs-dummy.o | sed -n 's, A ALT_START$$,,p')
$(TARGET).efi: guard = $(if $(filter y,$(XEN_BUILD_PE)),,:)

ifneq ($(build_id_linker),)
ifeq ($(call ld-ver-build-id,$(LD) $(filter -m%,$(EFI_LDFLAGS))),y)
CFLAGS += -DBUILD_ID_EFI
EFI_LDFLAGS += $(build_id_linker)
note_file := efi/buildid.o
# NB: this must be the last input in the linker call, because inputs following
# the -b option will all be treated as being in the specified format.
note_file_option := -b pe-x86-64 $(note_file)
else
note_file := note.o
endif
else
note_file :=
endif
note_file_option ?= $(note_file)

$(TARGET).efi: prelink-efi.o $(note_file) efi.lds efi/relocs-dummy.o efi/mkreloc
	$(foreach base, $(VIRT_BASE) $(ALT_BASE), \
	          $(guard) $(LD) $(call EFI_LDFLAGS,$(base)) -T efi.lds -N $< efi/relocs-dummy.o \
	                $(BASEDIR)/common/symbols-dummy.o $(note_file_option) -o $(@D)/.$(@F).$(base).0 &&) :
	$(guard) efi/mkreloc $(foreach base,$(VIRT_BASE) $(ALT_BASE),$(@D)/.$(@F).$(base).0) >$(@D)/.$(@F).0r.S
	$(guard) $(NM) -pa --format=sysv $(@D)/.$(@F).$(VIRT_BASE).0 \
		| $(guard) $(BASEDIR)/tools/symbols $(all_symbols) --sysv --sort >$(@D)/.$(@F).0s.S
	$(guard) $(MAKE) -f $(BASEDIR)/Rules.mk $(@D)/.$(@F).0r.o $(@D)/.$(@F).0s.o
	$(foreach base, $(VIRT_BASE) $(ALT_BASE), \
	          $(guard) $(LD) $(call EFI_LDFLAGS,$(base)) -T efi.lds -N $< \
	                $(@D)/.$(@F).0r.o $(@D)/.$(@F).0s.o $(note_file_option) -o $(@D)/.$(@F).$(base).1 &&) :
	$(guard) efi/mkreloc $(foreach base,$(VIRT_BASE) $(ALT_BASE),$(@D)/.$(@F).$(base).1) >$(@D)/.$(@F).1r.S
	$(guard) $(NM) -pa --format=sysv $(@D)/.$(@F).$(VIRT_BASE).1 \
		| $(guard) $(BASEDIR)/tools/symbols $(all_symbols) --sysv --sort >$(@D)/.$(@F).1s.S
	$(guard) $(MAKE) -f $(BASEDIR)/Rules.mk $(@D)/.$(@F).1r.o $(@D)/.$(@F).1s.o
	$(guard) $(LD) $(call EFI_LDFLAGS,$(VIRT_BASE)) -T efi.lds -N $< \
	                $(@D)/.$(@F).1r.o $(@D)/.$(@F).1s.o $(note_file_option) -o $@
	if $(guard) false; then rm -f $@; echo 'EFI support disabled'; \
	else $(NM) -pa --format=sysv $(@D)/$(@F) \
		| $(BASEDIR)/tools/symbols --xensyms --sysv --sort >$(@D)/$(@F).map; fi
	rm -f $(@D)/.$(@F).[0-9]* $(@D)/..$(@F).[0-9]*

efi/boot.init.o efi/runtime.o efi/compat.o efi/buildid.o: $(BASEDIR)/arch/x86/efi/built_in.o
efi/boot.init.o efi/runtime.o efi/compat.o efi/buildid.o: ;

$(BASEDIR)/include/asm-x86/asm-macros.h: asm-macros.i Makefile
	echo '#if 0' >$@.new
	echo '.if 0' >>$@.new
	echo '#endif' >>$@.new
	echo 'asm ( ".include \"$@\"" );' >>$@.new
	echo '#if 0' >>$@.new
	echo '.endif' >>$@.new
	cat $< >>$@.new
	echo '#endif' >>$@.new
	$(call move-if-changed,$@.new,$@)

efi/mkreloc: efi/mkreloc.c
	$(HOSTCC) $(HOSTCFLAGS) -g -o $@ $<

.PHONY: clean
clean::
	rm -f asm-offsets.s *.lds boot/*.o boot/*~ boot/core boot/mkelf32
	rm -f asm-macros.i $(BASEDIR)/include/asm-x86/asm-macros.*
	rm -f $(BASEDIR)/.xen-syms.[0-9]* boot/.*.d $(BASEDIR)/.xen.elf32
	rm -f $(BASEDIR)/.xen.efi.[0-9]* efi/*.efi efi/mkreloc
	rm -f boot/cmdline.S boot/reloc.S boot/*.lnk boot/*.bin
	rm -f note.o

# Suppress loading of DEPS files for internal, temporary target files.  This
# then also suppresses re-generation of the respective .*.d2 files.
ifeq ($(filter-out .xen%.o,$(notdir $(MAKECMDGOALS))),)
DEPS:=
endif
