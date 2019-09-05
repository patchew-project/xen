# Use Clang/LLVM instead of GCC?
clang     ?= n

# If we are not cross-compiling, default HOSTC{C/XX} to C{C/XX}
ifeq ($(XEN_TARGET_ARCH), $(XEN_COMPILE_ARCH))
HOSTCC    ?= $(CC)
HOSTCXX   ?= $(CXX)
endif

AS         = $(CROSS_COMPILE)as
LD         = $(CROSS_COMPILE)ld
ifeq ($(clang),y)
gcc       := n
CC         = $(CROSS_COMPILE)clang
CXX        = $(CROSS_COMPILE)clang++
LD_LTO     = $(CROSS_COMPILE)llvm-ld
HOSTCC    ?= clang
HOSTCXX   ?= clang++
else
gcc       := y
CC         = $(CROSS_COMPILE)gcc
CXX        = $(CROSS_COMPILE)g++
LD_LTO     = $(CROSS_COMPILE)ld
HOSTCC    ?= gcc
HOSTCXX   ?= g++
endif

CPP        = $(CC) -E
AR         = $(CROSS_COMPILE)ar
RANLIB     = $(CROSS_COMPILE)ranlib
NM         = $(CROSS_COMPILE)nm
STRIP      = $(CROSS_COMPILE)strip
OBJCOPY    = $(CROSS_COMPILE)objcopy
OBJDUMP    = $(CROSS_COMPILE)objdump
SIZEUTIL   = $(CROSS_COMPILE)size

# Allow git to be wrappered in the environment
GIT        ?= git

INSTALL      = install
INSTALL_DIR  = $(INSTALL) -d -m0755 -p
INSTALL_DATA = $(INSTALL) -m0644 -p
INSTALL_PROG = $(INSTALL) -m0755 -p

BOOT_DIR ?= /boot
DEBUG_DIR ?= /usr/lib/debug

SOCKET_LIBS =
UTIL_LIBS = -lutil

SONAME_LDFLAG = -soname
SHLIB_LDFLAGS = -shared

