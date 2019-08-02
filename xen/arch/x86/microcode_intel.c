/*
 * Intel CPU Microcode Update Driver for Linux
 *
 * Copyright (C) 2000-2006 Tigran Aivazian <tigran@aivazian.fsnet.co.uk>
 *               2006 Shaohua Li <shaohua.li@intel.com> *
 * This driver allows to upgrade microcode on Intel processors
 * belonging to IA-32 family - PentiumPro, Pentium II,
 * Pentium III, Xeon, Pentium 4, etc.
 *
 * Reference: Section 8.11 of Volume 3a, IA-32 Intel? Architecture
 * Software Developer's Manual
 * Order Number 253668 or free download from:
 *
 * http://developer.intel.com/design/pentium4/manuals/253668.htm
 *
 * For more information, go to http://www.urbanmyth.org/microcode
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <xen/lib.h>
#include <xen/kernel.h>
#include <xen/init.h>
#include <xen/sched.h>
#include <xen/smp.h>
#include <xen/spinlock.h>

#include <asm/msr.h>
#include <asm/processor.h>
#include <asm/microcode.h>

#define pr_debug(x...) ((void)0)

struct microcode_header_intel {
    unsigned int hdrver;
    unsigned int rev;
    union {
        struct {
            uint16_t year;
            uint8_t day;
            uint8_t month;
        };
        unsigned int date;
    };
    unsigned int sig;
    unsigned int cksum;
    unsigned int ldrver;
    unsigned int pf;
    unsigned int datasize;
    unsigned int totalsize;
    unsigned int reserved[3];
};

struct microcode_intel {
    struct microcode_header_intel hdr;
    unsigned int bits[0];
};

/* microcode format is extended from prescott processors */
struct extended_signature {
    unsigned int sig;
    unsigned int pf;
    unsigned int cksum;
};

struct extended_sigtable {
    unsigned int count;
    unsigned int cksum;
    unsigned int reserved[3];
    struct extended_signature sigs[0];
};

#define DEFAULT_UCODE_DATASIZE  (2000)
#define MC_HEADER_SIZE          (sizeof(struct microcode_header_intel))
#define DEFAULT_UCODE_TOTALSIZE (DEFAULT_UCODE_DATASIZE + MC_HEADER_SIZE)
#define EXT_HEADER_SIZE         (sizeof(struct extended_sigtable))
#define EXT_SIGNATURE_SIZE      (sizeof(struct extended_signature))
#define DWSIZE                  (sizeof(u32))
#define get_totalsize(mc) \
        (((struct microcode_intel *)mc)->hdr.totalsize ? \
         ((struct microcode_intel *)mc)->hdr.totalsize : \
         DEFAULT_UCODE_TOTALSIZE)

#define get_datasize(mc) \
        (((struct microcode_intel *)mc)->hdr.datasize ? \
         ((struct microcode_intel *)mc)->hdr.datasize : DEFAULT_UCODE_DATASIZE)

#define sigmatch(s1, s2, p1, p2) \
        (((s1) == (s2)) && (((p1) & (p2)) || (((p1) == 0) && ((p2) == 0))))

#define exttable_size(et) ((et)->count * EXT_SIGNATURE_SIZE + EXT_HEADER_SIZE)

/* serialize access to the physical write to MSR 0x79 */
static DEFINE_SPINLOCK(microcode_update_lock);

static int collect_cpu_info(struct cpu_signature *csig)
{
    unsigned int cpu_num = smp_processor_id();
    struct cpuinfo_x86 *c = &cpu_data[cpu_num];
    uint64_t msr_content;

    memset(csig, 0, sizeof(*csig));

    if ( (c->x86_vendor != X86_VENDOR_INTEL) || (c->x86 < 6) )
    {
        printk(KERN_ERR "microcode: CPU%d not a capable Intel "
               "processor\n", cpu_num);
        return -1;
    }

    csig->sig = cpuid_eax(0x00000001);

    if ( (c->x86_model >= 5) || (c->x86 > 6) )
    {
        /* get processor flags from MSR 0x17 */
        rdmsrl(MSR_IA32_PLATFORM_ID, msr_content);
        csig->pf = 1 << ((msr_content >> 50) & 7);
    }

    wrmsrl(MSR_IA32_UCODE_REV, 0x0ULL);
    /* As documented in the SDM: Do a CPUID 1 here */
    cpuid_eax(1);

    /* get the current revision from MSR 0x8B */
    rdmsrl(MSR_IA32_UCODE_REV, msr_content);
    csig->rev = (uint32_t)(msr_content >> 32);
    pr_debug("microcode: collect_cpu_info : sig=%#x, pf=%#x, rev=%#x\n",
             csig->sig, csig->pf, csig->rev);

    return 0;
}

static enum microcode_match_result microcode_update_match(
    const struct microcode_header_intel *mc_header, unsigned int sig,
    unsigned int pf, unsigned int rev)
{
    const struct extended_sigtable *ext_header;
    const struct extended_signature *ext_sig;
    unsigned long data_size = get_datasize(mc_header);
    unsigned int i;
    const void *end = (const void *)mc_header + get_totalsize(mc_header);

    if ( sigmatch(sig, mc_header->sig, pf, mc_header->pf) )
        return (mc_header->rev > rev) ? NEW_UCODE : OLD_UCODE;

    ext_header = (const void *)(mc_header + 1) + data_size;
    ext_sig = (const void *)(ext_header + 1);

    /*
     * Make sure there is enough space to hold an extended header and enough
     * array elements.
     */
    if ( (end < (const void *)ext_sig) ||
         (end < (const void *)(ext_sig + ext_header->count)) )
        return MIS_UCODE;

    for ( i = 0; i < ext_header->count; i++ )
        if ( sigmatch(sig, ext_sig[i].sig, pf, ext_sig[i].pf) )
            return (mc_header->rev > rev) ? NEW_UCODE : OLD_UCODE;

    return MIS_UCODE;
}

static int microcode_sanity_check(void *mc)
{
    struct microcode_header_intel *mc_header = mc;
    struct extended_sigtable *ext_header = NULL;
    struct extended_signature *ext_sig;
    unsigned long total_size, data_size, ext_table_size;
    unsigned int ext_sigcount = 0, i;
    uint32_t sum, orig_sum;

    total_size = get_totalsize(mc_header);
    data_size = get_datasize(mc_header);
    if ( (data_size + MC_HEADER_SIZE) > total_size )
    {
        printk(KERN_ERR "microcode: error! "
               "Bad data size in microcode data file\n");
        return -EINVAL;
    }

    if ( (mc_header->ldrver != 1) || (mc_header->hdrver != 1) )
    {
        printk(KERN_ERR "microcode: error! "
               "Unknown microcode update format\n");
        return -EINVAL;
    }
    ext_table_size = total_size - (MC_HEADER_SIZE + data_size);
    if ( ext_table_size )
    {
        if ( (ext_table_size < EXT_HEADER_SIZE) ||
             ((ext_table_size - EXT_HEADER_SIZE) % EXT_SIGNATURE_SIZE) )
        {
            printk(KERN_ERR "microcode: error! "
                   "Small exttable size in microcode data file\n");
            return -EINVAL;
        }
        ext_header = mc + MC_HEADER_SIZE + data_size;
        if ( ext_table_size != exttable_size(ext_header) )
        {
            printk(KERN_ERR "microcode: error! "
                   "Bad exttable size in microcode data file\n");
            return -EFAULT;
        }
        ext_sigcount = ext_header->count;
    }

    /* check extended table checksum */
    if ( ext_table_size )
    {
        uint32_t ext_table_sum = 0;
        uint32_t *ext_tablep = (uint32_t *)ext_header;

        i = ext_table_size / DWSIZE;
        while ( i-- )
            ext_table_sum += ext_tablep[i];
        if ( ext_table_sum )
        {
            printk(KERN_WARNING "microcode: aborting, "
                   "bad extended signature table checksum\n");
            return -EINVAL;
        }
    }

    /* calculate the checksum */
    orig_sum = 0;
    i = (MC_HEADER_SIZE + data_size) / DWSIZE;
    while ( i-- )
        orig_sum += ((uint32_t *)mc)[i];
    if ( orig_sum )
    {
        printk(KERN_ERR "microcode: aborting, bad checksum\n");
        return -EINVAL;
    }
    if ( !ext_table_size )
        return 0;
    /* check extended signature checksum */
    for ( i = 0; i < ext_sigcount; i++ )
    {
        ext_sig = (void *)ext_header + EXT_HEADER_SIZE +
            EXT_SIGNATURE_SIZE * i;
        sum = orig_sum
            - (mc_header->sig + mc_header->pf + mc_header->cksum)
            + (ext_sig->sig + ext_sig->pf + ext_sig->cksum);
        if ( sum )
        {
            printk(KERN_ERR "microcode: aborting, bad checksum\n");
            return -EINVAL;
        }
    }
    return 0;
}

static bool match_cpu(const struct microcode_patch *patch)
{
    const struct cpu_signature *sig = &this_cpu(cpu_sig);

    if ( !patch )
        return false;

    return microcode_update_match(&patch->mc_intel->hdr,
                                  sig->sig, sig->pf, sig->rev) == NEW_UCODE;
}

static void free_patch(void *mc)
{
    xfree(mc);
}

static enum microcode_match_result compare_patch(
    const struct microcode_patch *new, const struct microcode_patch *old)
{
    const struct microcode_header_intel *old_header = &old->mc_intel->hdr;

    return microcode_update_match(&new->mc_intel->hdr, old_header->sig,
                                  old_header->pf, old_header->rev);
}

/*
 * return 0 - no update found
 * return 1 - found update
 * return < 0 - error
 */
static int get_matching_microcode(const void *mc)
{
    struct cpu_signature *sig = &this_cpu(cpu_sig);
    const struct microcode_header_intel *mc_header = mc;
    unsigned long total_size = get_totalsize(mc_header);
    void *new_mc = xmalloc_bytes(total_size);
    struct microcode_patch *new_patch = xmalloc(struct microcode_patch);

    if ( !new_patch || !new_mc )
    {
        xfree(new_patch);
        xfree(new_mc);
        return -ENOMEM;
    }
    memcpy(new_mc, mc, total_size);
    new_patch->mc_intel = new_mc;

    /* Make sure that this patch covers current CPU */
    if ( microcode_update_match(&new_patch->mc_intel->hdr, sig->sig,
                                sig->pf, sig->rev) == MIS_UCODE )
    {
        microcode_free_patch(new_patch);
        return 0;
    }

    microcode_update_cache(new_patch);

    pr_debug("microcode: CPU%d found a matching microcode update with"
             " version %#x (current=%#x)\n",
             smp_processord_id(), mc_header->rev, sig->rev);

    return 1;
}

static int apply_microcode(const struct microcode_patch *patch)
{
    unsigned long flags;
    uint64_t msr_content;
    unsigned int val[2];
    unsigned int cpu_num = raw_smp_processor_id();
    struct cpu_signature *sig = &this_cpu(cpu_sig);
    const struct microcode_intel *mc_intel;

    if ( !match_cpu(patch) )
        return -EINVAL;

    mc_intel = patch->mc_intel;

    /* serialize access to the physical write to MSR 0x79 */
    spin_lock_irqsave(&microcode_update_lock, flags);

    /* write microcode via MSR 0x79 */
    wrmsrl(MSR_IA32_UCODE_WRITE, (unsigned long)mc_intel->bits);
    wrmsrl(MSR_IA32_UCODE_REV, 0x0ULL);

    /* As documented in the SDM: Do a CPUID 1 here */
    cpuid_eax(1);

    /* get the current revision from MSR 0x8B */
    rdmsrl(MSR_IA32_UCODE_REV, msr_content);
    val[1] = (uint32_t)(msr_content >> 32);

    spin_unlock_irqrestore(&microcode_update_lock, flags);
    if ( val[1] != mc_intel->hdr.rev )
    {
        printk(KERN_ERR "microcode: CPU%d update from revision "
               "%#x to %#x failed. Resulting revision is %#x.\n", cpu_num,
               sig->rev, mc_intel->hdr.rev, val[1]);
        return -EIO;
    }
    printk(KERN_INFO "microcode: CPU%d updated from revision "
           "%#x to %#x, date = %04x-%02x-%02x \n",
           cpu_num, sig->rev, val[1], mc_intel->hdr.year,
           mc_intel->hdr.month, mc_intel->hdr.day);
    sig->rev = val[1];

    return 0;
}

static long get_next_ucode_from_buffer(void **mc, const u8 *buf,
                                       unsigned long size, long offset)
{
    struct microcode_header_intel *mc_header;
    unsigned long total_size;

    /* No more data */
    if ( offset >= size )
        return 0;
    mc_header = (struct microcode_header_intel *)(buf + offset);
    total_size = get_totalsize(mc_header);

    if ( (offset + total_size) > size )
    {
        printk(KERN_ERR "microcode: error! Bad data in microcode data file\n");
        return -EINVAL;
    }

    *mc = xmalloc_bytes(total_size);
    if ( *mc == NULL )
    {
        printk(KERN_ERR "microcode: error! Can not allocate memory\n");
        return -ENOMEM;
    }
    memcpy(*mc, (const void *)(buf + offset), total_size);
    return offset + total_size;
}

static int cpu_request_microcode(const void *buf, size_t size)
{
    long offset = 0;
    int error = 0;
    void *mc;

    while ( (offset = get_next_ucode_from_buffer(&mc, buf, size, offset)) > 0 )
    {
        error = microcode_sanity_check(mc);
        if ( error )
            break;
        error = get_matching_microcode(mc);
        if ( error < 0 )
            break;
        /*
         * It's possible the data file has multiple matching ucode,
         * lets keep searching till the latest version
         */
        if ( error == 1 )
            error = 0;

        xfree(mc);
    }
    if ( offset > 0 )
        xfree(mc);
    if ( offset < 0 )
        error = offset;

    if ( !error && match_cpu(microcode_get_cache()) )
        error = apply_microcode(microcode_get_cache());

    return error;
}

static const struct microcode_ops microcode_intel_ops = {
    .cpu_request_microcode            = cpu_request_microcode,
    .collect_cpu_info                 = collect_cpu_info,
    .apply_microcode                  = apply_microcode,
    .free_patch                       = free_patch,
    .compare_patch                    = compare_patch,
    .match_cpu                        = match_cpu,
};

int __init microcode_init_intel(void)
{
    if ( boot_cpu_data.x86_vendor == X86_VENDOR_INTEL )
        microcode_ops = &microcode_intel_ops;
    return 0;
}
