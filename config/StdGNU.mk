# Use Clang/LLVM instead of GCC?
clang     ?= n

ifeq ($(clang),y)
gcc       := n
DEF_CC     = clang
DEF_CXX    = clang++
LD_LTO    ?= $(CROSS_COMPILE)llvm-ld
else
gcc       := y
DEF_CC     = gcc
DEF_CXX    = g++
LD_LTO    ?= $(CROSS_COMPILE)ld
endif

CC        ?= $(CROSS_COMPILE)$(DEF_CC)
CXX       ?= $(CROSS_COMPILE)$(DEF_CXX)

# If we are not cross-compiling, default HOSTC{C/XX} to C{C/XX}
# else use the default values if unset
ifeq ($(XEN_TARGET_ARCH), $(XEN_COMPILE_ARCH))
HOSTCC    ?= $(CC)
HOSTCXX   ?= $(CXX)
else
HOSTCC    ?= $(DEF_CC)
HOSTCXX   ?= $(DEF_CXX)
endif

AS        ?= $(CROSS_COMPILE)as
LD        ?= $(CROSS_COMPILE)ld
CPP       ?= $(CC) -E
AR        ?= $(CROSS_COMPILE)ar
RANLIB    ?= $(CROSS_COMPILE)ranlib
NM        ?= $(CROSS_COMPILE)nm
STRIP     ?= $(CROSS_COMPILE)strip
OBJCOPY   ?= $(CROSS_COMPILE)objcopy
OBJDUMP   ?= $(CROSS_COMPILE)objdump
SIZEUTIL  ?= $(CROSS_COMPILE)size

# Allow git to be wrappered in the environment
GIT        ?= git

INSTALL     ?= install
INSTALL_DIR  = $(INSTALL) -d -m0755 -p
INSTALL_DATA = $(INSTALL) -m0644 -p
INSTALL_PROG = $(INSTALL) -m0755 -p

BOOT_DIR ?= /boot
DEBUG_DIR ?= /usr/lib/debug

SOCKET_LIBS =
UTIL_LIBS = -lutil

SONAME_LDFLAG = -soname
SHLIB_LDFLAGS = -shared

