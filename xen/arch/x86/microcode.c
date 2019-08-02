/*
 * Intel CPU Microcode Update Driver for Linux
 *
 * Copyright (C) 2000-2006 Tigran Aivazian <tigran@aivazian.fsnet.co.uk>
 *               2006      Shaohua Li <shaohua.li@intel.com> *
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

#include <xen/cpu.h>
#include <xen/lib.h>
#include <xen/kernel.h>
#include <xen/init.h>
#include <xen/notifier.h>
#include <xen/sched.h>
#include <xen/smp.h>
#include <xen/softirq.h>
#include <xen/spinlock.h>
#include <xen/stop_machine.h>
#include <xen/tasklet.h>
#include <xen/guest_access.h>
#include <xen/earlycpio.h>
#include <xen/watchdog.h>

#include <asm/delay.h>
#include <asm/msr.h>
#include <asm/nmi.h>
#include <asm/processor.h>
#include <asm/setup.h>
#include <asm/microcode.h>

/*
 * Before performing a late microcode update on any thread, we
 * rendezvous all cpus in stop_machine context. The timeout for
 * waiting for cpu rendezvous is 30ms. It is the timeout used by
 * live patching
 */
#define MICROCODE_CALLIN_TIMEOUT_US 30000

/*
 * Timeout for each thread to complete update is set to 1s. It is a
 * conservative choice considering all possible interference.
 */
#define MICROCODE_UPDATE_TIMEOUT_US 1000000

static module_t __initdata ucode_mod;
static signed int __initdata ucode_mod_idx;
static bool_t __initdata ucode_mod_forced;
static unsigned int nr_cores;
static enum {
    LOADING_EXITED,
    LOADING_ENTERED,
    LOADING_ABORTED,
} loading_state;

/*
 * If we scan the initramfs.cpio for the early microcode code
 * and find it, then 'ucode_blob' will contain the pointer
 * and the size of said blob. It is allocated from Xen's heap
 * memory.
 */
struct ucode_mod_blob {
    void *data;
    size_t size;
};

static struct ucode_mod_blob __initdata ucode_blob;
/*
 * By default we will NOT parse the multiboot modules to see if there is
 * cpio image with the microcode images.
 */
static bool_t __initdata ucode_scan;

/* Protected by microcode_mutex */
static struct microcode_patch *microcode_cache;

void __init microcode_set_module(unsigned int idx)
{
    ucode_mod_idx = idx;
    ucode_mod_forced = 1;
}

/*
 * The format is '[<integer>|scan]'. Both options are optional.
 * If the EFI has forced which of the multiboot payloads is to be used,
 * no parsing will be attempted.
 */
static int __init parse_ucode(const char *s)
{
    const char *q = NULL;

    if ( ucode_mod_forced ) /* Forced by EFI */
       return 0;

    if ( !strncmp(s, "scan", 4) )
        ucode_scan = 1;
    else
        ucode_mod_idx = simple_strtol(s, &q, 0);

    return (q && *q) ? -EINVAL : 0;
}
custom_param("ucode", parse_ucode);

/*
 * 8MB ought to be enough.
 */
#define MAX_EARLY_CPIO_MICROCODE (8 << 20)

void __init microcode_scan_module(
    unsigned long *module_map,
    const multiboot_info_t *mbi)
{
    module_t *mod = (module_t *)__va(mbi->mods_addr);
    uint64_t *_blob_start;
    unsigned long _blob_size;
    struct cpio_data cd;
    long offset;
    const char *p = NULL;
    int i;

    ucode_blob.size = 0;
    if ( !ucode_scan )
        return;

    if ( boot_cpu_data.x86_vendor == X86_VENDOR_AMD )
        p = "kernel/x86/microcode/AuthenticAMD.bin";
    else if ( boot_cpu_data.x86_vendor == X86_VENDOR_INTEL )
        p = "kernel/x86/microcode/GenuineIntel.bin";
    else
        return;

    /*
     * Try all modules and see whichever could be the microcode blob.
     */
    for ( i = 1 /* Ignore dom0 kernel */; i < mbi->mods_count; i++ )
    {
        if ( !test_bit(i, module_map) )
            continue;

        _blob_start = bootstrap_map(&mod[i]);
        _blob_size = mod[i].mod_end;
        if ( !_blob_start )
        {
            printk("Could not map multiboot module #%d (size: %ld)\n",
                   i, _blob_size);
            continue;
        }
        cd.data = NULL;
        cd.size = 0;
        cd = find_cpio_data(p, _blob_start, _blob_size, &offset /* ignore */);
        if ( cd.data )
        {
                /*
                 * This is an arbitrary check - it would be sad if the blob
                 * consumed most of the memory and did not allow guests
                 * to launch.
                 */
                if ( cd.size > MAX_EARLY_CPIO_MICROCODE )
                {
                    printk("Multiboot %d microcode payload too big! (%ld, we can do %d)\n",
                           i, cd.size, MAX_EARLY_CPIO_MICROCODE);
                    goto err;
                }
                ucode_blob.size = cd.size;
                ucode_blob.data = xmalloc_bytes(cd.size);
                if ( !ucode_blob.data )
                    cd.data = NULL;
                else
                    memcpy(ucode_blob.data, cd.data, cd.size);
        }
        bootstrap_map(NULL);
        if ( cd.data )
            break;
    }
    return;
err:
    bootstrap_map(NULL);
}
void __init microcode_grab_module(
    unsigned long *module_map,
    const multiboot_info_t *mbi)
{
    module_t *mod = (module_t *)__va(mbi->mods_addr);

    if ( ucode_mod_idx < 0 )
        ucode_mod_idx += mbi->mods_count;
    if ( ucode_mod_idx <= 0 || ucode_mod_idx >= mbi->mods_count ||
         !__test_and_clear_bit(ucode_mod_idx, module_map) )
        goto scan;
    ucode_mod = mod[ucode_mod_idx];
scan:
    if ( ucode_scan )
        microcode_scan_module(module_map, mbi);
}

const struct microcode_ops *microcode_ops;

static DEFINE_SPINLOCK(microcode_mutex);

DEFINE_PER_CPU(struct cpu_signature, cpu_sig);

/*
 * Count the CPUs that have entered, exited the rendezvous and succeeded in
 * microcode update during late microcode update respectively.
 *
 * Note that a bitmap is used for callin to allow cpu to set a bit multiple
 * times. It is required to do busy-loop in #NMI handling.
 */
static cpumask_t cpu_callin_map;
static atomic_t cpu_out, cpu_updated;

/*
 * Return a patch that covers current CPU. If there are multiple patches,
 * return the one with the highest revision number. Return error If no
 * patch is found and an error occurs during the parsing process. Otherwise
 * return NULL.
 */
static struct microcode_patch *microcode_parse_blob(const char *buf,
                                                    uint32_t len)
{
    if ( likely(!microcode_ops->collect_cpu_info(&this_cpu(cpu_sig))) )
        return microcode_ops->cpu_request_microcode(buf, len);

    return NULL;
}

void microcode_free_patch(struct microcode_patch *microcode_patch)
{
    microcode_ops->free_patch(microcode_patch->mc);
    xfree(microcode_patch);
}

/* Return true if cache gets updated. Otherwise, return false */
bool microcode_update_cache(struct microcode_patch *patch)
{

    ASSERT(spin_is_locked(&microcode_mutex));

    if ( !microcode_cache )
        microcode_cache = patch;
    else if ( microcode_ops->compare_patch(patch, microcode_cache) ==
                  NEW_UCODE )
    {
        microcode_free_patch(microcode_cache);
        microcode_cache = patch;
    }
    else
    {
        microcode_free_patch(patch);
        return false;
    }

    return true;
}

/*
 * Wait for a condition to be met with a timeout (us).
 */
static int wait_for_condition(int (*func)(void *data), void *data,
                         unsigned int timeout)
{
    while ( !func(data) )
    {
        if ( !timeout-- )
        {
            printk("CPU%u: Timeout in %s\n", smp_processor_id(), __func__);
            return -EBUSY;
        }
        udelay(1);
    }

    return 0;
}

static int wait_cpu_callin(void *nr)
{
    return cpumask_weight(&cpu_callin_map) >= (unsigned long)nr;
}

static int wait_cpu_callout(void *nr)
{
    return atomic_read(&cpu_out) >= (unsigned long)nr;
}

/*
 * Load a microcode update to current CPU.
 *
 * If no patch is provided, the cached patch will be loaded. Microcode update
 * during APs bringup and CPU resuming falls into this case.
 */
static int microcode_update_cpu(const struct microcode_patch *patch)
{
    int err = microcode_ops->collect_cpu_info(&this_cpu(cpu_sig));

    if ( unlikely(err) )
        return err;

    spin_lock(&microcode_mutex);

    if ( patch )
    {
        /*
         * If a patch is specified, it should has newer revision than
         * that of the patch cached.
         */
        if ( microcode_cache &&
             microcode_ops->compare_patch(patch, microcode_cache) != NEW_UCODE )
        {
            spin_unlock(&microcode_mutex);
            return -EINVAL;
        }
    }
    else if ( microcode_cache )
        patch = microcode_cache;
    else
        /* No patch to update */
        err = -ENOENT;

    if ( patch )
    {
        err = microcode_ops->apply_microcode(patch);
        /* clean up patch cache if we failed to load the cached patch */
        if ( patch == microcode_cache && err == -EIO )
        {
            microcode_free_patch(microcode_cache);
            microcode_cache = NULL;
        }
    }

    spin_unlock(&microcode_mutex);

    return err;
}

static int do_microcode_update(void *patch)
{
    unsigned int cpu = smp_processor_id();
    unsigned int cpu_nr = num_online_cpus();
    int ret;

    /* Mark loading an ucode is in progress */
    cmpxchg(&loading_state, LOADING_EXITED, LOADING_ENTERED);
    cpumask_set_cpu(cpu, &cpu_callin_map);
    ret = wait_for_condition(wait_cpu_callin, (void *)(unsigned long)cpu_nr,
                             MICROCODE_CALLIN_TIMEOUT_US);
    if ( ret )
    {
        cmpxchg(&loading_state, LOADING_ENTERED, LOADING_ABORTED);
        return ret;
    }

    /*
     * Load microcode update on only one logical processor per core, or in
     * AMD's term, one core per compute unit. The one with the lowest thread
     * id among all siblings is chosen to perform the loading.
     */
    if ( (cpu == cpumask_first(per_cpu(cpu_sibling_mask, cpu))) )
    {
        static unsigned int panicked = 0;
        bool monitor;
        unsigned int done;
        unsigned long tick = 0;

        ret = microcode_ops->apply_microcode(patch);
        if ( !ret )
        {
            unsigned int cpu2;

            atomic_inc(&cpu_updated);
            /* Propagate revision number to all siblings */
            for_each_cpu(cpu2, per_cpu(cpu_sibling_mask, cpu))
                per_cpu(cpu_sig, cpu2).rev = this_cpu(cpu_sig).rev;
        }

        /*
         * The first CPU reaching here will monitor the progress and emit
         * warning message if the duration is too long (e.g. >1 second).
         */
        monitor = !atomic_inc_return(&cpu_out);
        if ( monitor )
            tick = rdtsc_ordered();

        /* Waiting for all cores or computing units finishing update */
        done = atomic_read(&cpu_out);
        while ( panicked && done != nr_cores )
        {
            /*
             * During each timeout interval, at least a CPU is expected to
             * finish its update. Otherwise, something goes wrong.
             *
             * Note that RDTSC (in wait_for_condition()) is safe for threads to
             * execute while waiting for completion of loading an update.
             */
            if ( wait_for_condition(&wait_cpu_callout,
                                    (void *)(unsigned long)(done + 1),
                                    MICROCODE_UPDATE_TIMEOUT_US) &&
                 !cmpxchg(&panicked, 0, 1) )
                panic("Timeout when finishing updating microcode (finished %u/%u)",
                      done, nr_cores);

            /* Print warning message once if long time is spent here */
            if ( monitor )
            {
                if ( rdtsc_ordered() - tick >= cpu_khz * 1000 )
                {
                    printk(XENLOG_WARNING "WARNING: UPDATING MICROCODE HAS CONSUMED MORE THAN 1 SECOND!\n");
                    monitor = false;
                }
            }

            done = atomic_read(&cpu_out);
        }

        /* Mark loading is done to unblock other threads */
        loading_state = LOADING_EXITED;
    }
    else
    {
        while ( loading_state == LOADING_ENTERED )
            rep_nop();
    }

    if ( microcode_ops->end_update )
        microcode_ops->end_update();

    return ret;
}

static int microcode_nmi_callback(const struct cpu_user_regs *regs, int cpu)
{
    bool print = false;

    /* The first thread of a core is to load an update. Don't block it. */
    if ( cpu == cpumask_first(per_cpu(cpu_sibling_mask, cpu)) )
        return 0;

    if ( loading_state == LOADING_ENTERED )
    {
        cpumask_set_cpu(cpu, &cpu_callin_map);
        printk(XENLOG_DEBUG "CPU%u enters %s\n", smp_processor_id(), __func__);
        print = true;
    }

    while ( loading_state == LOADING_ENTERED )
        rep_nop();

    if ( print )
        printk(XENLOG_DEBUG "CPU%u exits %s\n", smp_processor_id(), __func__);

    return 0;
}

int microcode_update(XEN_GUEST_HANDLE_PARAM(const_void) buf, unsigned long len)
{
    int ret;
    void *buffer;
    unsigned int cpu, updated;
    struct microcode_patch *patch;
    nmi_callback_t *saved_nmi_callback;

    if ( len != (uint32_t)len )
        return -E2BIG;

    if ( microcode_ops == NULL )
        return -EINVAL;

    buffer = xmalloc_bytes(len);
    if ( !buffer )
        return -ENOMEM;

    if ( copy_from_guest(buffer, buf, len) )
    {
        ret = -EFAULT;
        goto free;
    }

    /* cpu_online_map must not change during update */
    if ( !get_cpu_maps() )
    {
        ret = -EBUSY;
        goto free;
    }

    if ( microcode_ops->start_update )
    {
        ret = microcode_ops->start_update();
        if ( ret != 0 )
            goto put;
    }

    patch = microcode_parse_blob(buffer, len);
    if ( IS_ERR(patch) )
    {
        ret = PTR_ERR(patch);
        printk(XENLOG_INFO "Parsing microcode blob error %d\n", ret);
        goto put;
    }

    if ( !patch )
    {
        printk(XENLOG_INFO "No ucode found. Update aborted!\n");
        ret = -EINVAL;
        goto put;
    }

    cpumask_clear(&cpu_callin_map);
    atomic_set(&cpu_out, 0);
    atomic_set(&cpu_updated, 0);
    loading_state = LOADING_EXITED;

    /* Calculate the number of online CPU core */
    nr_cores = 0;
    for_each_online_cpu(cpu)
        if ( cpu == cpumask_first(per_cpu(cpu_sibling_mask, cpu)) )
            nr_cores++;

    printk(XENLOG_INFO "%u cores are to update their microcode\n", nr_cores);

    /*
     * We intend to disable interrupt for long time, which may lead to
     * watchdog timeout.
     */
    watchdog_disable();

    saved_nmi_callback = set_nmi_callback(microcode_nmi_callback);
    /*
     * Late loading dance. Why the heavy-handed stop_machine effort?
     *
     * - HT siblings must be idle and not execute other code while the other
     *   sibling is loading microcode in order to avoid any negative
     *   interactions cause by the loading.
     *
     * - In addition, microcode update on the cores must be serialized until
     *   this requirement can be relaxed in the future. Right now, this is
     *   conservative and good.
     */
    ret = stop_machine_run(do_microcode_update, patch, NR_CPUS);
    set_nmi_callback(saved_nmi_callback);
    watchdog_enable();

    updated = atomic_read(&cpu_updated);
    if ( updated > 0 )
    {
        spin_lock(&microcode_mutex);
        microcode_update_cache(patch);
        spin_unlock(&microcode_mutex);
    }
    else
        microcode_free_patch(patch);

    if ( updated && updated != nr_cores )
        printk(XENLOG_ERR
               "ERROR: Updating microcode succeeded on %u cores and failed on\n"
               "other %u cores. A system with differing microcode revisions is\n"
               "considered unstable. Please reboot and do not load the microcode\n"
               "that triggers this warning!\n", updated, nr_cores - updated);

 put:
    put_cpu_maps();
 free:
    xfree(buffer);
    return ret;
}

static int __init microcode_init(void)
{
    /*
     * At this point, all CPUs should have updated their microcode
     * via the early_microcode_* paths so free the microcode blob.
     */
    if ( ucode_blob.size )
    {
        xfree(ucode_blob.data);
        ucode_blob.size = 0;
        ucode_blob.data = NULL;
    }
    else if ( ucode_mod.mod_end )
    {
        bootstrap_map(NULL);
        ucode_mod.mod_end = 0;
    }

    return 0;
}
__initcall(microcode_init);

/* Load a cached update to current cpu */
int microcode_update_one(void)
{
    return microcode_ops ? microcode_update_cpu(NULL) : 0;
}

/*
 * BSP calls this function to parse ucode blob and then apply an update.
 */
int __init early_microcode_update_cpu(void)
{
    int rc = 0;
    void *data = NULL;
    size_t len;

    if ( !microcode_ops )
        return -ENOSYS;

    if ( ucode_blob.size )
    {
        len = ucode_blob.size;
        data = ucode_blob.data;
    }
    else if ( ucode_mod.mod_end )
    {
        len = ucode_mod.mod_end;
        data = bootstrap_map(&ucode_mod);
    }

    if ( data )
    {
        struct microcode_patch *patch;

        if ( microcode_ops->start_update )
            rc = microcode_ops->start_update();

        if ( rc )
            return rc;

        patch = microcode_parse_blob(data, len);
        if ( IS_ERR(patch) )
        {
            printk(XENLOG_INFO "Parsing microcode blob error %ld\n",
                   PTR_ERR(patch));
            return PTR_ERR(patch);
        }

        if ( !patch )
        {
            printk(XENLOG_INFO "No ucode found. Update aborted!\n");
            return -EINVAL;
        }

        spin_lock(&microcode_mutex);
        rc = microcode_update_cache(patch);
        spin_unlock(&microcode_mutex);

        ASSERT(rc);

        return microcode_update_cpu(NULL);
    }
    else
        return -ENOMEM;
}

int __init early_microcode_init(void)
{
    int rc;

    rc = microcode_init_intel();
    if ( rc )
        return rc;

    rc = microcode_init_amd();
    if ( rc )
        return rc;

    if ( microcode_ops )
    {
        microcode_ops->collect_cpu_info(&this_cpu(cpu_sig));

        if ( ucode_mod.mod_end || ucode_blob.size )
            rc = early_microcode_update_cpu();
    }

    return rc;
}
