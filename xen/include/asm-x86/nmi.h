
#ifndef ASM_NMI_H
#define ASM_NMI_H

#include <public/nmi.h>

struct cpu_user_regs;

/* Watchdog boolean from the command line */
extern bool opt_watchdog;

/* Watchdog force parameter from the command line */
extern bool watchdog_force;

/* CPU to handle platform NMI */
extern const unsigned int nmi_cpu;
 
typedef int nmi_callback_t(const struct cpu_user_regs *regs, int cpu);
 
/** 
 * set_nmi_callback
 *
 * Set a handler for an NMI. Only one handler may be
 * set. Return the old nmi callback handler.
 */
nmi_callback_t *set_nmi_callback(nmi_callback_t *callback);
 
/** 
 * unset_nmi_callback
 *
 * Remove the handler previously set.
 */
void unset_nmi_callback(void);

DECLARE_PER_CPU(unsigned int, nmi_count);

typedef void nmi_contfunc_t(void *arg);

/**
 * set_nmi_continuation
 *
 * Schedule a function to be started in interrupt context after NMI handling.
 */
int set_nmi_continuation(nmi_contfunc_t *func, void *arg);

/* Check for NMI continuation pending. */
bool nmi_check_continuation(void);
#endif /* ASM_NMI_H */
