#  -*- mode: Makefile; -*-

# `all' is the default target
all:

-include $(XEN_ROOT)/config/Tools.mk
include $(XEN_ROOT)/Config.mk

export _INSTALL := $(INSTALL)
INSTALL = $(XEN_ROOT)/tools/cross-install

LDFLAGS += $(PREPEND_LDFLAGS_XEN_TOOLS)

XEN_INCLUDE        = $(XEN_ROOT)/tools/include

LIBS_LIBS += toolcore
USELIBS_toolcore :=
LIBS_LIBS += toollog
USELIBS_toollog :=
LIBS_LIBS += evtchn
USELIBS_evtchn := toollog toolcore
LIBS_LIBS += gnttab
USELIBS_gnttab := toollog toolcore
LIBS_LIBS += call
USELIBS_call := toollog toolcore
LIBS_LIBS += foreignmemory
USELIBS_foreignmemory := toollog toolcore
LIBS_LIBS += devicemodel
USELIBS_devicemodel := toollog toolcore call
LIBS_LIBS += hypfs
USELIBS_hypfs := toollog toolcore call

XEN_libxenctrl     = $(XEN_ROOT)/tools/libxc
# Currently libxenguest lives in the same directory as libxenctrl
XEN_libxenguest    = $(XEN_libxenctrl)
XEN_libxenlight    = $(XEN_ROOT)/tools/libxl
# Currently libxlutil lives in the same directory as libxenlight
XEN_libxlutil      = $(XEN_libxenlight)
XEN_libxenstore    = $(XEN_ROOT)/tools/xenstore
XEN_libxenstat     = $(XEN_ROOT)/tools/xenstat/libxenstat/src
XEN_libxenvchan    = $(XEN_ROOT)/tools/libvchan

CFLAGS_xeninclude = -I$(XEN_INCLUDE)

XENSTORE_XENSTORED ?= y

# A debug build of tools?
debug ?= y
debug_symbols ?= $(debug)

XEN_GOCODE_URL    = golang.xenproject.org

ifeq ($(debug_symbols),y)
CFLAGS += -g3
endif

ifneq ($(nosharedlibs),y)
INSTALL_SHLIB = $(INSTALL_PROG)
SYMLINK_SHLIB = ln -sf
libextension = .so
else
libextension = .a
XENSTORE_STATIC_CLIENTS=y
# If something tries to use these it is a mistake.  Provide references
# to nonexistent programs to produce a sane error message.
INSTALL_SHLIB = : install-shlib-unsupported-fail
SYMLINK_SHLIB = : symlink-shlib-unsupported-fail
endif

# Compiling and linking against in tree libraries.
#
# In order to compile and link against an in-tree library various
# cpp/compiler/linker options are required.
#
# For example consider a library "libfoo" which itself uses two other
# libraries:
#  libbar - whose use is entirely internal to libfoo and not exposed
#           to users of libfoo at all.
#  libbaz - whose use is entirely internal to libfoo but libfoo's
#           public headers include one or more of libbaz's
#           public headers. Users of libfoo are therefore transitively
#           using libbaz's header but not linking against libbaz.
#
# SHDEPS_libfoo: Flags for linking recursive dependencies of
#                libfoo. Must contain SHLIB for every library which
#                libfoo links against. So must contain both
#                $(SHLIB_libbar) and $(SHLIB_libbaz).
#
# SHLIB_libfoo: Flags for recursively linking against libfoo. Must
#               contains SHDEPS_libfoo and:
#                   -Wl,-rpath-link=<directory containing libfoo.so>
#
# CFLAGS_libfoo: Flags for compiling against libfoo. Must add the
#                directories containing libfoo's headers to the
#                include path. Must recursively include
#                $(CFLAGS_libbaz), to satisfy the transitive inclusion
#                of the headers but not $(CFLAGS_libbar) since none of
#                libbar's headers are required to build against
#                libfoo.
#
# LDLIBS_libfoo: Flags for linking against libfoo. Must contain
#                $(SHDEPS_libfoo) and the path to libfoo.so
#
# Consumers of libfoo should include $(CFLAGS_libfoo) and
# $(LDLIBS_libfoo) in their appropriate directories. They should not
# include any CFLAGS or LDLIBS relating to libbar or libbaz unless
# they use those libraries directly (not via libfoo) too.
#
# Consumers of libfoo should not directly use $(SHDEPS_libfoo) or
# $(SHLIB_libfoo)

$(foreach lib,$(LIBS_LIBS),$(eval XEN_libxen$(lib) = $(XEN_ROOT)/tools/libs/$(lib)))
$(foreach lib,$(LIBS_LIBS),$(eval CFLAGS_libxen$(lib) = -I$(XEN_libxen$(lib))/include $(CFLAGS_xeninclude)))
$(foreach lib,$(LIBS_LIBS),$(eval SHDEPS_libxen$(lib) = $(foreach use,$(USELIBS_$(lib)),$(SHLIB_libxen$(use)))))
$(foreach lib,$(LIBS_LIBS),$(eval LDLIBS_libxen$(lib) = $(SHDEPS_libxen$(lib)) $(XEN_libxen$(lib))/libxen$(lib)$(libextension)))
$(foreach lib,$(LIBS_LIBS),$(eval SHLIB_libxen$(lib) = $(SHDEPS_libxen$(lib)) -Wl,-rpath-link=$(XEN_libxen$(lib))))

# code which compiles against libxenctrl get __XEN_TOOLS__ and
# therefore sees the unstable hypercall interfaces.
CFLAGS_libxenctrl = -I$(XEN_libxenctrl)/include $(CFLAGS_libxentoollog) $(CFLAGS_libxenforeignmemory) $(CFLAGS_libxendevicemodel) $(CFLAGS_xeninclude) -D__XEN_TOOLS__
SHDEPS_libxenctrl = $(SHLIB_libxentoollog) $(SHLIB_libxenevtchn) $(SHLIB_libxengnttab) $(SHLIB_libxencall) $(SHLIB_libxenforeignmemory) $(SHLIB_libxendevicemodel)
LDLIBS_libxenctrl = $(SHDEPS_libxenctrl) $(XEN_libxenctrl)/libxenctrl$(libextension)
SHLIB_libxenctrl  = $(SHDEPS_libxenctrl) -Wl,-rpath-link=$(XEN_libxenctrl)

CFLAGS_libxenguest = -I$(XEN_libxenguest)/include $(CFLAGS_libxenevtchn) $(CFLAGS_libxenforeignmemory) $(CFLAGS_xeninclude)
SHDEPS_libxenguest = $(SHLIB_libxenevtchn)
LDLIBS_libxenguest = $(SHDEPS_libxenguest) $(XEN_libxenguest)/libxenguest$(libextension)
SHLIB_libxenguest  = $(SHDEPS_libxenguest) -Wl,-rpath-link=$(XEN_libxenguest)

CFLAGS_libxenstore = -I$(XEN_libxenstore)/include $(CFLAGS_xeninclude)
SHDEPS_libxenstore = $(SHLIB_libxentoolcore)
LDLIBS_libxenstore = $(SHDEPS_libxenstore) $(XEN_libxenstore)/libxenstore$(libextension)
SHLIB_libxenstore  = $(SHDEPS_libxenstore) -Wl,-rpath-link=$(XEN_libxenstore)
ifeq ($(CONFIG_Linux),y)
LDLIBS_libxenstore += -ldl
endif

CFLAGS_libxenstat  = -I$(XEN_libxenstat)
SHDEPS_libxenstat  = $(SHLIB_libxenctrl) $(SHLIB_libxenstore)
LDLIBS_libxenstat  = $(SHDEPS_libxenstat) $(XEN_libxenstat)/libxenstat$(libextension)
SHLIB_libxenstat   = $(SHDEPS_libxenstat) -Wl,-rpath-link=$(XEN_libxenstat)

CFLAGS_libxenvchan = -I$(XEN_libxenvchan) $(CFLAGS_libxengnttab) $(CFLAGS_libxenevtchn)
SHDEPS_libxenvchan = $(SHLIB_libxentoollog) $(SHLIB_libxenstore) $(SHLIB_libxenevtchn) $(SHLIB_libxengnttab)
LDLIBS_libxenvchan = $(SHDEPS_libxenvchan) $(XEN_libxenvchan)/libxenvchan$(libextension)
SHLIB_libxenvchan  = $(SHDEPS_libxenvchan) -Wl,-rpath-link=$(XEN_libxenvchan)

ifeq ($(debug),y)
# Disable optimizations
CFLAGS += -O0 -fno-omit-frame-pointer
# But allow an override to -O0 in case Python enforces -D_FORTIFY_SOURCE=<n>.
PY_CFLAGS += $(PY_NOOPT_CFLAGS)
else
CFLAGS += -O2 -fomit-frame-pointer
endif

CFLAGS_libxenlight = -I$(XEN_libxenlight) $(CFLAGS_libxenctrl) $(CFLAGS_xeninclude)
SHDEPS_libxenlight = $(SHLIB_libxenctrl) $(SHLIB_libxenstore) $(SHLIB_libxenhypfs)
LDLIBS_libxenlight = $(SHDEPS_libxenlight) $(XEN_libxenlight)/libxenlight$(libextension)
SHLIB_libxenlight  = $(SHDEPS_libxenlight) -Wl,-rpath-link=$(XEN_libxenlight)

CFLAGS_libxlutil = -I$(XEN_libxlutil)
SHDEPS_libxlutil = $(SHLIB_libxenlight)
LDLIBS_libxlutil = $(SHDEPS_libxlutil) $(XEN_libxlutil)/libxlutil$(libextension)
SHLIB_libxlutil  = $(SHDEPS_libxlutil) -Wl,-rpath-link=$(XEN_libxlutil)

CFLAGS += -D__XEN_INTERFACE_VERSION__=__XEN_LATEST_INTERFACE_VERSION__

# Get gcc to generate the dependencies for us.
CFLAGS += -MMD -MP -MF .$(if $(filter-out .,$(@D)),$(subst /,@,$(@D))@)$(@F).d
DEPS = .*.d

ifneq ($(FILE_OFFSET_BITS),)
CFLAGS  += -D_FILE_OFFSET_BITS=$(FILE_OFFSET_BITS)
endif
ifneq ($(XEN_OS),NetBSD)
# Enable implicit LFS support *and* explicit LFS names.
CFLAGS  += -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
endif

# 32-bit x86 does not perform well with -ve segment accesses on Xen.
CFLAGS-$(CONFIG_X86_32) += $(call cc-option,$(CC),-mno-tls-direct-seg-refs)
CFLAGS += $(CFLAGS-y)

CFLAGS += $(EXTRA_CFLAGS_XEN_TOOLS)

INSTALL_PYTHON_PROG = \
	$(XEN_ROOT)/tools/python/install-wrap "$(PYTHON_PATH)" $(INSTALL_PROG)

%.opic: %.c
	$(CC) $(CPPFLAGS) -DPIC $(CFLAGS) $(CFLAGS_$*.opic) -fPIC -c -o $@ $< $(APPEND_CFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_$*.o) -c -o $@ $< $(APPEND_CFLAGS)

%.o: %.cc
	$(CC) $(CPPFLAGS) $(CXXFLAGS) $(CXXFLAGS_$*.o) -c -o $@ $< $(APPEND_CFLAGS)

%.o: %.S
	$(CC) $(CFLAGS) $(CFLAGS_$*.o) -c $< -o $@ $(APPEND_CFLAGS)
%.opic: %.S
	$(CC) $(CPPFLAGS) -DPIC $(CFLAGS) $(CFLAGS.opic) -fPIC -c -o $@ $< $(APPEND_CFLAGS)

subdirs-all subdirs-clean subdirs-install subdirs-distclean subdirs-uninstall: .phony
	@set -e; for subdir in $(SUBDIRS) $(SUBDIRS-y); do \
		$(MAKE) subdir-$(patsubst subdirs-%,%,$@)-$$subdir; \
	done

subdir-all-% subdir-clean-% subdir-install-% subdir-uninstall-%: .phony
	$(MAKE) -C $* $(patsubst subdir-%-$*,%,$@)

subdir-distclean-%: .phony
	$(MAKE) -C $* distclean

no-configure-targets := distclean subdir-distclean% clean subdir-clean% subtree-force-update-all %-dir-force-update
ifeq (,$(filter $(no-configure-targets),$(MAKECMDGOALS)))
$(XEN_ROOT)/config/Tools.mk:
	$(error You have to run ./configure before building or installing the tools)
endif

PKG_CONFIG_DIR ?= $(XEN_ROOT)/tools/pkg-config

PKG_CONFIG_FILTER = $(foreach l,$(PKG_CONFIG_REMOVE),-e 's!\([ ,]\)$(l),!\1!g' -e 's![ ,]$(l)$$!!g')

$(PKG_CONFIG_DIR)/%.pc: %.pc.in Makefile $(XEN_ROOT)/tools/Rules.mk
	mkdir -p $(PKG_CONFIG_DIR)
	@sed -e 's!@@version@@!$(PKG_CONFIG_VERSION)!g' \
	     -e 's!@@prefix@@!$(PKG_CONFIG_PREFIX)!g' \
	     -e 's!@@incdir@@!$(PKG_CONFIG_INCDIR)!g' \
	     -e 's!@@libdir@@!$(PKG_CONFIG_LIBDIR)!g' \
	     -e 's!@@firmwaredir@@!$(XENFIRMWAREDIR)!g' \
	     -e 's!@@libexecbin@@!$(LIBEXEC_BIN)!g' \
	     -e 's!@@cflagslocal@@!$(PKG_CONFIG_CFLAGS_LOCAL)!g' \
	     -e 's!@@libsflag@@\([^ ]*\)!-L\1 -Wl,-rpath-link=\1!g' \
	     $(PKG_CONFIG_FILTER) < $< > $@

%.pc: %.pc.in Makefile $(XEN_ROOT)/tools/Rules.mk
	@sed -e 's!@@version@@!$(PKG_CONFIG_VERSION)!g' \
	     -e 's!@@prefix@@!$(PKG_CONFIG_PREFIX)!g' \
	     -e 's!@@incdir@@!$(PKG_CONFIG_INCDIR)!g' \
	     -e 's!@@libdir@@!$(PKG_CONFIG_LIBDIR)!g' \
	     -e 's!@@firmwaredir@@!$(XENFIRMWAREDIR)!g' \
	     -e 's!@@libexecbin@@!$(LIBEXEC_BIN)!g' \
	     -e 's!@@cflagslocal@@!!g' \
	     -e 's!@@libsflag@@!-L!g' \
	     $(PKG_CONFIG_FILTER) < $< > $@
