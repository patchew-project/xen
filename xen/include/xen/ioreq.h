/*
 * ioreq.h: Hardware virtual machine assist interface definitions.
 *
 * Copyright (c) 2016 Citrix Systems Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __XEN_IOREQ_H__
#define __XEN_IOREQ_H__

#include <xen/sched.h>

struct ioreq_page {
    gfn_t gfn;
    struct page_info *page;
    void *va;
};

struct ioreq_vcpu {
    struct list_head list_entry;
    struct vcpu      *vcpu;
    evtchn_port_t    ioreq_evtchn;
    bool             pending;
};

#define NR_IO_RANGE_TYPES (XEN_DMOP_IO_RANGE_PCI + 1)
#define MAX_NR_IO_RANGES  256

struct ioreq_server {
    struct domain          *target, *emulator;

    /* Lock to serialize toolstack modifications */
    spinlock_t             lock;

    struct ioreq_page      ioreq;
    struct list_head       ioreq_vcpu_list;
    struct ioreq_page      bufioreq;

    /* Lock to serialize access to buffered ioreq ring */
    spinlock_t             bufioreq_lock;
    evtchn_port_t          bufioreq_evtchn;
    struct rangeset        *range[NR_IO_RANGE_TYPES];
    bool                   enabled;
    uint8_t                bufioreq_handling;
};

#ifdef CONFIG_IOREQ_SERVER
static inline bool domain_has_ioreq_server(const struct domain *d)
{
    ASSERT((current->domain == d) || atomic_read(&d->pause_count));

    return d->ioreq_server.nr_servers;
}
#else
static inline bool domain_has_ioreq_server(const struct domain *d)
{
    return false;
}
#endif

struct ioreq_server *get_ioreq_server(const struct domain *d,
                                      unsigned int id);

static inline paddr_t ioreq_mmio_first_byte(const ioreq_t *p)
{
    return unlikely(p->df) ?
           p->addr - (p->count - 1ul) * p->size :
           p->addr;
}

static inline paddr_t ioreq_mmio_last_byte(const ioreq_t *p)
{
    unsigned long size = p->size;

    return unlikely(p->df) ?
           p->addr + size - 1:
           p->addr + (p->count * size) - 1;
}

static inline bool ioreq_needs_completion(const ioreq_t *ioreq)
{
    return ioreq->state == STATE_IOREQ_READY &&
           !ioreq->data_is_ptr &&
           (ioreq->type != IOREQ_TYPE_PIO || ioreq->dir != IOREQ_WRITE);
}

bool io_pending(struct vcpu *v);
bool handle_io_completion(struct vcpu *v);
bool is_ioreq_server_page(struct domain *d, const struct page_info *page);

int create_ioreq_server(struct domain *d, int bufioreq_handling,
                        ioservid_t *id);
int destroy_ioreq_server(struct domain *d, ioservid_t id);
int get_ioreq_server_info(struct domain *d, ioservid_t id,
                          unsigned long *ioreq_gfn,
                          unsigned long *bufioreq_gfn,
                          evtchn_port_t *bufioreq_port);
int get_ioreq_server_frame(struct domain *d, ioservid_t id,
                           unsigned long idx, mfn_t *mfn);
int map_io_range_to_ioreq_server(struct domain *d, ioservid_t id,
                                 uint32_t type, uint64_t start,
                                 uint64_t end);
int unmap_io_range_from_ioreq_server(struct domain *d, ioservid_t id,
                                     uint32_t type, uint64_t start,
                                     uint64_t end);
int set_ioreq_server_state(struct domain *d, ioservid_t id,
                           bool enabled);

int all_ioreq_servers_add_vcpu(struct domain *d, struct vcpu *v);
void all_ioreq_servers_remove_vcpu(struct domain *d, struct vcpu *v);
void destroy_all_ioreq_servers(struct domain *d);

struct ioreq_server *select_ioreq_server(struct domain *d,
                                         ioreq_t *p);
int send_ioreq(struct ioreq_server *s, ioreq_t *proto_p,
               bool buffered);
unsigned int broadcast_ioreq(ioreq_t *p, bool buffered);

void ioreq_init(struct domain *d);

#endif /* __XEN_IOREQ_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
