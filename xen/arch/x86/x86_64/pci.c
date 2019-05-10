/******************************************************************************
 * pci.c
 * 
 * Architecture-dependent PCI access functions.
 */

#include <xen/spinlock.h>
#include <xen/pci.h>
#include <asm/io.h>

#define PCI_CONF_ADDRESS(bus, dev, func, reg) \
    (0x80000000 | (bus << 16) | (dev << 11) | (func << 8) | (reg & ~3))

#define GEN_PCI_CONF_READ(s)                                                   \
    uint ## s ## _t pci_conf_read ## s (unsigned int seg, unsigned int bus,    \
                                        unsigned int dev, unsigned int func,   \
                                        unsigned int reg)                      \
    {                                                                          \
        uint32_t value;                                                        \
                                                                               \
        BUILD_BUG_ON(s != 8 && s != 16 && s != 32);                            \
        if ( seg || reg > 255 )                                                \
            pci_mmcfg_read(seg, bus, PCI_DEVFN(dev, func), reg, s / 8, &value);\
        else                                                                   \
        {                                                                      \
            BUG_ON((bus > 255) || (dev > 31) || (func > 7));                   \
            value = pci_conf_read(PCI_CONF_ADDRESS(bus, dev, func, reg),       \
                                  reg & (4 - s / 8), s / 8);                   \
        }                                                                      \
                                                                               \
        return value;                                                          \
    }

/* Grep fodder */
#define pci_conf_read8
#define pci_conf_read16
#define pci_conf_read32

#undef pci_conf_read8
#undef pci_conf_read16
#undef pci_conf_read32

GEN_PCI_CONF_READ(8)
GEN_PCI_CONF_READ(16)
GEN_PCI_CONF_READ(32)

#undef GEN_PCI_CONF_READ

#define GEN_PCI_CONF_WRITE(s)                                                  \
    void pci_conf_write ## s (unsigned int seg, unsigned int bus,              \
                              unsigned int dev, unsigned int func,             \
                              unsigned int reg, uint ## s ## _t data)          \
    {                                                                          \
        BUILD_BUG_ON(s != 8 && s != 16 && s != 32);                            \
        if ( seg || reg > 255 )                                                \
            pci_mmcfg_write(seg, bus, PCI_DEVFN(dev, func), reg, s / 8, data); \
        else                                                                   \
        {                                                                      \
            BUG_ON((bus > 255) || (dev > 31) || (func > 7));                   \
            pci_conf_write(PCI_CONF_ADDRESS(bus, dev, func, reg),              \
                           reg & (4 - s / 8), s / 8, data);                    \
        }                                                                      \
    }

/* Grep fodder */
#define pci_conf_write8
#define pci_conf_write16
#define pci_conf_write32

#undef pci_conf_write8
#undef pci_conf_write16
#undef pci_conf_write32

GEN_PCI_CONF_WRITE(8)
GEN_PCI_CONF_WRITE(16)
GEN_PCI_CONF_WRITE(32)

#undef GEN_PCI_CONF_WRITE

