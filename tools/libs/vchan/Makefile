XEN_ROOT = $(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

MAJOR = 4.14
MINOR = 0
LIBNAME  := vchan

CFLAGS += $(CFLAGS_libxenctrl)

LIBHEADER := libxenvchan.h

SRCS-y += init.c
SRCS-y += io.c

include $(XEN_ROOT)/tools/libs/libs.mk

$(PKG_CONFIG_LOCAL): PKG_CONFIG_INCDIR = $(XEN_libxenvchan)/include
$(PKG_CONFIG_LOCAL): PKG_CONFIG_CFLAGS_LOCAL = $(CFLAGS_xeninclude)
