#ifndef ASM_X86__MICROCODE_H
#define ASM_X86__MICROCODE_H

#include <xen/percpu.h>

enum microcode_match_result {
    OLD_UCODE, /* signature matched, but revision id is older or equal */
    NEW_UCODE, /* signature matched, but revision id is newer */
    MIS_UCODE, /* signature mismatched */
};

struct cpu_signature;

struct microcode_patch {
    union {
        struct microcode_intel *mc_intel;
        struct microcode_amd *mc_amd;
        void *mc;
    };
};

struct microcode_ops {
    int (*cpu_request_microcode)(const void *buf, size_t size);
    int (*collect_cpu_info)(struct cpu_signature *csig);
    int (*apply_microcode)(void);
    int (*start_update)(void);
    void (*end_update)(void);
    void (*free_patch)(void *mc);
    bool (*match_cpu)(const struct microcode_patch *patch);
    enum microcode_match_result (*compare_patch)(
            const struct microcode_patch *new,
            const struct microcode_patch *old);
};

struct cpu_signature {
    unsigned int sig;
    unsigned int pf;
    unsigned int rev;
};

DECLARE_PER_CPU(struct cpu_signature, cpu_sig);
extern const struct microcode_ops *microcode_ops;

const struct microcode_patch *microcode_get_cache(void);
bool microcode_update_cache(struct microcode_patch *patch);
void microcode_free_patch(struct microcode_patch *patch);

#endif /* ASM_X86__MICROCODE_H */
