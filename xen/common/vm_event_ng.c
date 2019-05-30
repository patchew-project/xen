/******************************************************************************
 * vm_event_ng.c
 *
 * VM event support (new generation).
 *
 * Copyright (c) 2019, Bitdefender S.R.L.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include <xen/sched.h>
#include <xen/event.h>
#include <xen/vm_event.h>
#include <xen/vmap.h>
#include <asm/monitor.h>
#include <asm/vm_event.h>
#include <xsm/xsm.h>

#define to_channels(_ved) container_of((_ved), \
                                        struct vm_event_channels_domain, ved)

#define VM_EVENT_CHANNELS_ENABLED       1

struct vm_event_channels_domain
{
    /* VM event domain */
    struct vm_event_domain ved;
    /* shared channels buffer */
    struct vm_event_slot *slots;
    /* the buffer size (number of frames) */
    unsigned int nr_frames;
    /* state */
    bool enabled;
    /* buffer's mnf list */
    mfn_t mfn[0];
};

static const struct vm_event_ops vm_event_channels_ops;

static int vm_event_channels_alloc_buffer(struct vm_event_channels_domain *impl)
{
    int i, rc = -ENOMEM;

    for ( i = 0; i < impl->nr_frames; i++ )
    {
        struct page_info *page = alloc_domheap_page(impl->ved.d, 0);
        if ( !page )
            goto err;

        if ( !get_page_and_type(page, impl->ved.d, PGT_writable_page) )
        {
            rc = -ENODATA;
            goto err;
        }

        impl->mfn[i] = page_to_mfn(page);
    }

    impl->slots = (struct vm_event_slot *)vmap(impl->mfn, impl->nr_frames);
    if ( !impl->slots )
        goto err;

    for ( i = 0; i < impl->nr_frames; i++ )
        clear_page((void*)impl->slots + i * PAGE_SIZE);

    return 0;

err:
    while ( --i >= 0 )
    {
        struct page_info *page = mfn_to_page(impl->mfn[i]);

        if ( test_and_clear_bit(_PGC_allocated, &page->count_info) )
            put_page(page);
        put_page_and_type(page);
    }

    return rc;
}

static void vm_event_channels_free_buffer(struct vm_event_channels_domain *impl)
{
    int i;

    ASSERT(impl);

    if ( !impl->slots )
        return;

    vunmap(impl->slots);

    for ( i = 0; i < impl->nr_frames; i++ )
    {
        struct page_info *page = mfn_to_page(impl->mfn[i]);

        ASSERT(page);
        if ( test_and_clear_bit(_PGC_allocated, &page->count_info) )
            put_page(page);
        put_page_and_type(page);
    }
}

static int vm_event_channels_create(
    struct domain *d,
    struct xen_domctl_vm_event_ng_op *vec,
    struct vm_event_domain **_ved,
    int pause_flag,
    xen_event_channel_notification_t notification_fn)
{
    int rc, i;
    unsigned int nr_frames = PFN_UP(d->max_vcpus * sizeof(struct vm_event_slot));
    struct vm_event_channels_domain *impl;

    if ( *_ved )
        return -EBUSY;

    impl = _xzalloc(sizeof(struct vm_event_channels_domain) +
                           nr_frames * sizeof(mfn_t),
                    __alignof__(struct vm_event_channels_domain));
    if ( unlikely(!impl) )
        return -ENOMEM;

    spin_lock_init(&impl->ved.lock);
    spin_lock(&impl->ved.lock);

    impl->nr_frames = nr_frames;
    impl->ved.d = d;
    impl->ved.ops = &vm_event_channels_ops;

    rc = vm_event_init_domain(d);
    if ( rc < 0 )
        goto err;

    rc = vm_event_channels_alloc_buffer(impl);
    if ( rc )
        goto err;

    for ( i = 0; i < d->max_vcpus; i++ )
    {
        rc = alloc_unbound_xen_event_channel(d, i, current->domain->domain_id,
                                             notification_fn);
        if ( rc < 0 )
            goto err;

        impl->slots[i].port = rc;
        impl->slots[i].state = STATE_VM_EVENT_SLOT_IDLE;
    }

    impl->enabled = false;

    spin_unlock(&impl->ved.lock);
    *_ved = &impl->ved;
    return 0;

err:
    spin_unlock(&impl->ved.lock);
    XFREE(impl);
    return rc;
}

static int vm_event_channels_destroy(struct vm_event_domain **_ved)
{
    struct vcpu *v;
    struct vm_event_channels_domain *impl = to_channels(*_ved);
    int i;

    spin_lock(&(*_ved)->lock);

    for_each_vcpu( (*_ved)->d, v )
    {
        if ( atomic_read(&v->vm_event_pause_count) )
            vm_event_vcpu_unpause(v);
    }

    for ( i = 0; i < (*_ved)->d->max_vcpus; i++ )
        evtchn_close((*_ved)->d, impl->slots[i].port, 0);

    vm_event_channels_free_buffer(impl);
    spin_unlock(&(*_ved)->lock);
    XFREE(*_ved);

    return 0;
}

static bool vm_event_channels_check(struct vm_event_domain *ved)
{
    return to_channels(ved)->slots != NULL;
}

static void vm_event_channels_cleanup(struct vm_event_domain **_ved)
{
    vm_event_channels_destroy(_ved);
}

static int vm_event_channels_claim_slot(struct vm_event_domain *ved,
                                        bool allow_sleep)
{
    return 0;
}

static void vm_event_channels_cancel_slot(struct vm_event_domain *ved)
{
}

static void vm_event_channels_put_request(struct vm_event_domain *ved,
                                          vm_event_request_t *req)
{
    struct vm_event_channels_domain *impl = to_channels(ved);
    struct vm_event_slot *slot;

    /* exit if the vm_event_domain was not specifically enabled */
    if ( !impl->enabled )
        return;

    ASSERT( req->vcpu_id >= 0 && req->vcpu_id < ved->d->max_vcpus );

    slot = &impl->slots[req->vcpu_id];

    if ( current->domain != ved->d )
    {
        req->flags |= VM_EVENT_FLAG_FOREIGN;
#ifndef NDEBUG
        if ( !(req->flags & VM_EVENT_FLAG_VCPU_PAUSED) )
            gdprintk(XENLOG_G_WARNING, "d%dv%d was not paused.\n",
                     ved->d->domain_id, req->vcpu_id);
#endif
    }

    req->version = VM_EVENT_INTERFACE_VERSION;

    spin_lock(&impl->ved.lock);
    if ( slot->state != STATE_VM_EVENT_SLOT_IDLE )
    {
        gdprintk(XENLOG_G_WARNING, "The VM event slot for d%dv%d is not IDLE.\n",
                 impl->ved.d->domain_id, req->vcpu_id);
        spin_unlock(&impl->ved.lock);
        return;
    }

    slot->u.req = *req;
    slot->state = STATE_VM_EVENT_SLOT_SUBMIT;
    spin_unlock(&impl->ved.lock);
    notify_via_xen_event_channel(impl->ved.d, slot->port);
}

static int vm_event_channels_get_response(struct vm_event_channels_domain *impl,
                                          struct vcpu *v, vm_event_response_t *rsp)
{
    struct vm_event_slot *slot = &impl->slots[v->vcpu_id];

    ASSERT( slot != NULL );
    spin_lock(&impl->ved.lock);

    if ( slot->state != STATE_VM_EVENT_SLOT_FINISH )
    {
        gdprintk(XENLOG_G_WARNING, "The VM event slot state for d%dv%d is invalid.\n",
                 impl->ved.d->domain_id, v->vcpu_id);
        spin_unlock(&impl->ved.lock);
        return -1;
    }

    *rsp = slot->u.rsp;
    slot->state = STATE_VM_EVENT_SLOT_IDLE;

    spin_unlock(&impl->ved.lock);
    return 0;
}

static int vm_event_channels_resume(struct vm_event_channels_domain *impl,
                                    struct vcpu *v)
{
    vm_event_response_t rsp;

    if ( unlikely(!impl || !vm_event_check(&impl->ved)) )
         return -ENODEV;

    ASSERT(impl->ved.d != current->domain);

    if ( vm_event_channels_get_response(impl, v, &rsp) ||
         rsp.version != VM_EVENT_INTERFACE_VERSION ||
         rsp.vcpu_id != v->vcpu_id )
        return -1;

    vm_event_handle_response(impl->ved.d, v, &rsp);

    return 0;
}

/* Registered with Xen-bound event channel for incoming notifications. */
static void monitor_notification(struct vcpu *v, unsigned int port)
{
    vm_event_channels_resume(to_channels(v->domain->vm_event_monitor), v);
}

int vm_event_ng_domctl(struct domain *d, struct xen_domctl_vm_event_ng_op *vec,
                       XEN_GUEST_HANDLE_PARAM(void) u_domctl)
{
    int rc;

    if ( vec->op == XEN_VM_EVENT_NG_GET_VERSION )
    {
        vec->u.version = VM_EVENT_INTERFACE_VERSION;
        return 0;
    }

    if ( unlikely(d == NULL) )
        return -ESRCH;

    rc = xsm_vm_event_control(XSM_PRIV, d, vec->type, vec->op);
    if ( rc )
        return rc;

    if ( unlikely(d == current->domain) ) /* no domain_pause() */
    {
        gdprintk(XENLOG_INFO, "Tried to do a memory event op on itself.\n");
        return -EINVAL;
    }

    if ( unlikely(d->is_dying) )
    {
        gdprintk(XENLOG_INFO, "Ignoring memory event op on dying domain %u\n",
                 d->domain_id);
        return 0;
    }

    if ( unlikely(d->vcpu == NULL) || unlikely(d->vcpu[0] == NULL) )
    {
        gdprintk(XENLOG_INFO,
                 "Memory event op on a domain (%u) with no vcpus\n",
                 d->domain_id);
        return -EINVAL;
    }

    switch ( vec->type )
    {
    case XEN_VM_EVENT_TYPE_MONITOR:
    {
        rc = -EINVAL;

        switch ( vec-> op)
        {
        case XEN_VM_EVENT_NG_CREATE:
            /* domain_pause() not required here, see XSA-99 */
            rc = arch_monitor_init_domain(d);
            if ( rc )
                break;
            rc = vm_event_channels_create(d, vec, &d->vm_event_monitor,
                                     _VPF_mem_access, monitor_notification);
            break;

        case XEN_VM_EVENT_NG_DESTROY:
            if ( !vm_event_check(d->vm_event_monitor) )
                break;
            domain_pause(d);
            rc = vm_event_channels_destroy(&d->vm_event_monitor);
            arch_monitor_cleanup_domain(d);
            domain_unpause(d);
            break;

        case XEN_VM_EVENT_NG_SET_STATE:
            if ( !vm_event_check(d->vm_event_monitor) )
                break;
            domain_pause(d);
            to_channels(d->vm_event_monitor)->enabled = !!vec->u.enabled;
            domain_unpause(d);
            rc = 0;
            break;

        default:
            rc = -ENOSYS;
        }
        break;
    }

#ifdef CONFIG_HAS_MEM_PAGING
    case XEN_VM_EVENT_TYPE_PAGING:
#endif

#ifdef CONFIG_HAS_MEM_SHARING
    case XEN_VM_EVENT_TYPE_SHARING:
#endif

    default:
        rc = -ENOSYS;
    }

    return rc;
}

int vm_event_ng_get_frames(struct domain *d, unsigned int id,
                           unsigned long frame, unsigned int nr_frames,
                           xen_pfn_t mfn_list[])
{
    struct vm_event_domain *ved;
    int i;

    switch (id )
    {
    case XEN_VM_EVENT_TYPE_MONITOR:
        ved = d->vm_event_monitor;
        break;

    default:
        return -ENOSYS;
    }

    if ( !vm_event_check(ved) )
        return -EINVAL;

    if ( frame != 0 || nr_frames != to_channels(ved)->nr_frames )
        return -EINVAL;

    spin_lock(&ved->lock);

    for ( i = 0; i < to_channels(ved)->nr_frames; i++ )
        mfn_list[i] = mfn_x(to_channels(ved)->mfn[i]);

    spin_unlock(&ved->lock);
    return 0;
}

static const struct vm_event_ops vm_event_channels_ops = {
    .check = vm_event_channels_check,
    .cleanup = vm_event_channels_cleanup,
    .claim_slot = vm_event_channels_claim_slot,
    .cancel_slot = vm_event_channels_cancel_slot,
    .put_request = vm_event_channels_put_request
};

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
