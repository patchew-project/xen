#ifndef __ASM_DOMAIN_H__
#define __ASM_DOMAIN_H__

#include <xen/cache.h>
#include <xen/sched.h>
#include <asm/page.h>
#include <asm/p2m.h>
#include <public/hvm/params.h>
#include <xen/serial.h>
#include <xen/rbtree.h>

struct hvm_domain
{
    uint64_t              params[HVM_NR_PARAMS];
};

#ifdef CONFIG_RISCV_64
enum domain_type {
    DOMAIN_32BIT,
    DOMAIN_64BIT,
};
#define is_32bit_domain(d) ((d)->arch.type == DOMAIN_32BIT)
#define is_64bit_domain(d) ((d)->arch.type == DOMAIN_64BIT)
#else
#define is_32bit_domain(d) (1)
#define is_64bit_domain(d) (0)
#endif

/* The hardware domain has always its memory direct mapped. */
#define is_domain_direct_mapped(d) ((d) == hardware_domain)

struct vtimer {
        struct vcpu *v;
        int irq;
        struct timer timer;
        uint32_t ctl;
        uint64_t cval;
};

struct arch_domain
{
    /* Virtual MMU */
    struct p2m_domain p2m;

    struct hvm_domain hvm;

}  __cacheline_aligned;

struct arch_vcpu
{

    /*
     * Points into ->stack, more convenient than doing pointer arith
     * all the time.
     */
    struct cpu_info *cpu_info;

}  __cacheline_aligned;

void vcpu_show_execution_state(struct vcpu *);
void vcpu_show_registers(const struct vcpu *);

static inline struct vcpu_guest_context *alloc_vcpu_guest_context(void)
{
    return xmalloc(struct vcpu_guest_context);
}

static inline void free_vcpu_guest_context(struct vcpu_guest_context *vgc)
{
    xfree(vgc);
}

static inline void arch_vcpu_block(struct vcpu *v) {}

#endif /* __ASM_DOMAIN_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
