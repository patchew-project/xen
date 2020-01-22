########################################
# riscv-specific definitions

#
# If you change any of these configuration options then you must
# 'make clean' before rebuilding.
#

CFLAGS += -I$(BASEDIR)/include

$(call cc-options-add,CFLAGS,CC,$(EMBEDDED_EXTRA_CFLAGS))
$(call cc-option-add,CFLAGS,CC,-Wnested-externs)

EARLY_PRINTK := n

ifeq ($(CONFIG_DEBUG),y)

# See docs/misc/arm/early-printk.txt for syntax

EARLY_PRINTK := 8250,0x1c021000,2

ifneq ($(EARLY_PRINTK_$(CONFIG_EARLY_PRINTK)),)
EARLY_PRINTK_CFG := $(subst $(comma), ,$(EARLY_PRINTK_$(CONFIG_EARLY_PRINTK)))
else
EARLY_PRINTK_CFG := $(subst $(comma), ,$(CONFIG_EARLY_PRINTK))
endif

# Extract configuration from string
EARLY_PRINTK_INC := $(word 1,$(EARLY_PRINTK_CFG))
EARLY_UART_BASE_ADDRESS := $(word 2,$(EARLY_PRINTK_CFG))

# UART specific options
ifeq ($(EARLY_PRINTK_INC),8250)
EARLY_UART_REG_SHIFT := $(word 3,$(EARLY_PRINTK_CFG))
endif

ifneq ($(EARLY_PRINTK_INC),)
EARLY_PRINTK := y
endif

CFLAGS-$(EARLY_PRINTK) += -DCONFIG_EARLY_PRINTK
CFLAGS-$(EARLY_PRINTK_INIT_UART) += -DEARLY_PRINTK_INIT_UART
CFLAGS-$(EARLY_PRINTK) += -DEARLY_PRINTK_INC=\"debug-$(EARLY_PRINTK_INC).inc\"
CFLAGS-$(EARLY_PRINTK) += -DEARLY_PRINTK_BAUD=$(EARLY_PRINTK_BAUD)
CFLAGS-$(EARLY_PRINTK) += -DEARLY_UART_BASE_ADDRESS=$(EARLY_UART_BASE_ADDRESS)
CFLAGS-$(EARLY_PRINTK) += -DEARLY_UART_REG_SHIFT=$(EARLY_UART_REG_SHIFT)

else # !CONFIG_DEBUG

ifneq ($(CONFIG_EARLY_PRINTK),)
# Early printk is dependant on a debug build.
$(error CONFIG_EARLY_PRINTK enabled for non-debug build)
endif

endif
