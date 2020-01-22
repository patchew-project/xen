/******************************************************************************
 * arch-riscv.h
 *
 * Guest OS interface to RISC-V Xen.
 * Initially based on the ARM implementation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright 2019 (C) Alistair Francis <alistair.francis@wdc.com>
 */

#ifndef __XEN_PUBLIC_ARCH_RISCV_H__
#define __XEN_PUBLIC_ARCH_RISCV_H__

#define  int64_aligned_t  int64_t __attribute__((aligned(8)))
#define uint64_aligned_t uint64_t __attribute__((aligned(8)))

#ifndef __ASSEMBLY__
#define ___DEFINE_XEN_GUEST_HANDLE(name, type)                  \
    typedef union { type *p; unsigned long q; }                 \
        __guest_handle_ ## name;                                \
    typedef union { type *p; uint64_aligned_t q; }              \
        __guest_handle_64_ ## name

/*
 * XEN_GUEST_HANDLE represents a guest pointer, when passed as a field
 * in a struct in memory. On RISCV is always 8 bytes sizes and 8 bytes
 * aligned.
 * XEN_GUEST_HANDLE_PARAM represents a guest pointer, when passed as an
 * hypercall argument. It is 4 bytes on aarch32 and 8 bytes on aarch64.
 */
#define __DEFINE_XEN_GUEST_HANDLE(name, type) \
    ___DEFINE_XEN_GUEST_HANDLE(name, type);   \
    ___DEFINE_XEN_GUEST_HANDLE(const_##name, const type)
#define DEFINE_XEN_GUEST_HANDLE(name)   __DEFINE_XEN_GUEST_HANDLE(name, name)
#define __XEN_GUEST_HANDLE(name)        __guest_handle_64_ ## name
#define XEN_GUEST_HANDLE(name)          __XEN_GUEST_HANDLE(name)
#define XEN_GUEST_HANDLE_PARAM(name)    __guest_handle_ ## name
#define set_xen_guest_handle_raw(hnd, val)                  \
    do {                                                    \
        typeof(&(hnd)) _sxghr_tmp = &(hnd);                 \
        _sxghr_tmp->q = 0;                                  \
        _sxghr_tmp->p = val;                                \
    } while ( 0 )
#define set_xen_guest_handle(hnd, val) set_xen_guest_handle_raw(hnd, val)

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
/* Anonymous union includes both 32- and 64-bit names (e.g., r0/x0). */
# define __DECL_REG(n64, n32) union {          \
        uint64_t n64;                          \
        uint32_t n32;                          \
    }
#else
/* Non-gcc sources must always use the proper 64-bit name (e.g., x0). */
#define __DECL_REG(n64, n32) uint64_t n64
#endif

struct vcpu_guest_core_regs
{
    unsigned long zero;
    unsigned long ra;
    unsigned long sp;
    unsigned long gp;
    unsigned long tp;
    unsigned long t0;
    unsigned long t1;
    unsigned long t2;
    unsigned long s0;
    unsigned long s1;
    unsigned long a0;
    unsigned long a1;
    unsigned long a2;
    unsigned long a3;
    unsigned long a4;
    unsigned long a5;
    unsigned long a6;
    unsigned long a7;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;
    unsigned long t3;
    unsigned long t4;
    unsigned long t5;
    unsigned long t6;
    unsigned long sepc;
    unsigned long sstatus;
    unsigned long hstatus;
    unsigned long sp_exec;

    unsigned long hedeleg;
    unsigned long hideleg;
    unsigned long bsstatus;
    unsigned long bsie;
    unsigned long bstvec;
    unsigned long bsscratch;
    unsigned long bsepc;
    unsigned long bscause;
    unsigned long bstval;
    unsigned long bsip;
    unsigned long bsatp;
};
typedef struct vcpu_guest_core_regs vcpu_guest_core_regs_t;
DEFINE_XEN_GUEST_HANDLE(vcpu_guest_core_regs_t);

typedef uint64_t xen_pfn_t;
#define PRI_xen_pfn PRIx64
#define PRIu_xen_pfn PRIu64

typedef uint64_t xen_ulong_t;
#define PRI_xen_ulong PRIx64

#if defined(__XEN__) || defined(__XEN_TOOLS__)

struct vcpu_guest_context {
};
typedef struct vcpu_guest_context vcpu_guest_context_t;
DEFINE_XEN_GUEST_HANDLE(vcpu_guest_context_t);

struct xen_arch_domainconfig {
};

struct arch_vcpu_info {
};
typedef struct arch_vcpu_info arch_vcpu_info_t;

struct arch_shared_info {
};
typedef struct arch_shared_info arch_shared_info_t;

typedef uint64_t xen_callback_t;

#endif

/* Maximum number of virtual CPUs in legacy multi-processor guests. */
/* Only one. All other VCPUS must use VCPUOP_register_vcpu_info */
#define XEN_LEGACY_MAX_VCPUS 1

/* Current supported guest VCPUs */
#define GUEST_MAX_VCPUS 128

#endif /* __ASSEMBLY__ */

#ifndef __ASSEMBLY__
/* Stub definition of PMU structure */
typedef struct xen_pmu_arch { uint8_t dummy; } xen_pmu_arch_t;
#endif

#endif /*  __XEN_PUBLIC_ARCH_RISCV_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
