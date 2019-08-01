#ifndef ASM_X86__MICROCODE_H
#define ASM_X86__MICROCODE_H

#include <xen/percpu.h>

enum microcode_match_result {
    OLD_UCODE, /* signature matched, but revision id is older or equal */
    NEW_UCODE, /* signature matched, but revision id is newer */
    MIS_UCODE, /* signature mismatched */
};

struct cpu_signature;
struct ucode_cpu_info;

struct microcode_ops {
    int (*microcode_resume_match)(unsigned int cpu, const void *mc);
    int (*cpu_request_microcode)(unsigned int cpu, const void *buf,
                                 size_t size);
    int (*collect_cpu_info)(unsigned int cpu, struct cpu_signature *csig);
    int (*apply_microcode)(unsigned int cpu);
    int (*start_update)(void);
};

struct cpu_signature {
    unsigned int sig;
    unsigned int pf;
    unsigned int rev;
};

struct ucode_cpu_info {
    struct cpu_signature cpu_sig;
    union {
        struct microcode_intel *mc_intel;
        struct microcode_amd *mc_amd;
        void *mc_valid;
    } mc;
};

DECLARE_PER_CPU(struct ucode_cpu_info, ucode_cpu_info);
extern const struct microcode_ops *microcode_ops;

#endif /* ASM_X86__MICROCODE_H */
