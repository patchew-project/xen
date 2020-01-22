#ifndef __ARM_REGS_H__
#define __ARM_REGS_H__

#define PSR_MODE_MASK 0x1f

#ifndef __ASSEMBLY__

#include <xen/lib.h>
#include <xen/types.h>
#include <public/xen.h>
#include <asm/current.h>
#include <asm/processor.h>

#define hyp_mode(r)     (0)

static inline bool guest_mode(const struct cpu_user_regs *r)
{
    unsigned long diff = (char *)guest_cpu_user_regs() - (char *)(r);
    /* Frame pointer must point into current CPU stack. */
    ASSERT(diff < STACK_SIZE);
    /* If not a guest frame, it must be a hypervisor frame. */
    ASSERT((diff == 0) || hyp_mode(r));
    /* Return TRUE if it's a guest frame. */
    return (diff == 0);
}

#define return_reg(v) ((v)->arch.cpu_info->guest_cpu_user_regs.r0)

register_t get_user_reg(struct cpu_user_regs *regs, int reg);
void set_user_reg(struct cpu_user_regs *regs, int reg, register_t val);

#endif

#endif /* __ARM_REGS_H__ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
