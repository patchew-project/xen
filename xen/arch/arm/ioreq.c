/*
 * arm/ioreq.c: hardware virtual machine I/O emulation
 *
 * Copyright (c) 2019 Arm ltd.
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

#include <xen/domain.h>
#include <xen/ioreq.h>

#include <asm/traps.h>

#include <public/hvm/ioreq.h>

enum io_state handle_ioserv(struct cpu_user_regs *regs, struct vcpu *v)
{
    const union hsr hsr = { .bits = regs->hsr };
    const struct hsr_dabt dabt = hsr.dabt;
    /* Code is similar to handle_read */
    uint8_t size = (1 << dabt.size) * 8;
    register_t r = v->io.io_req.data;

    /* We are done with the IO */
    v->io.io_req.state = STATE_IOREQ_NONE;

    if ( dabt.write )
        return IO_HANDLED;

    /*
     * Sign extend if required.
     * Note that we expect the read handler to have zeroed the bits
     * outside the requested access size.
     */
    if ( dabt.sign && (r & (1UL << (size - 1))) )
    {
        /*
         * We are relying on register_t using the same as
         * an unsigned long in order to keep the 32-bit assembly
         * code smaller.
         */
        BUILD_BUG_ON(sizeof(register_t) != sizeof(unsigned long));
        r |= (~0UL) << size;
    }

    set_user_reg(regs, dabt.reg, r);

    return IO_HANDLED;
}

enum io_state try_fwd_ioserv(struct cpu_user_regs *regs,
                             struct vcpu *v, mmio_info_t *info)
{
    struct vcpu_io *vio = &v->io;
    ioreq_t p = {
        .type = IOREQ_TYPE_COPY,
        .addr = info->gpa,
        .size = 1 << info->dabt.size,
        .count = 1,
        .dir = !info->dabt.write,
        /*
         * On x86, df is used by 'rep' instruction to tell the direction
         * to iterate (forward or backward).
         * On Arm, all the accesses to MMIO region will do a single
         * memory access. So for now, we can safely always set to 0.
         */
        .df = 0,
        .data = get_user_reg(regs, info->dabt.reg),
        .state = STATE_IOREQ_READY,
    };
    struct ioreq_server *s = NULL;
    enum io_state rc;

    switch ( vio->io_req.state )
    {
    case STATE_IOREQ_NONE:
        break;

    case STATE_IORESP_READY:
        return IO_HANDLED;

    default:
        gdprintk(XENLOG_ERR, "wrong state %u\n", vio->io_req.state);
        return IO_ABORT;
    }

    s = select_ioreq_server(v->domain, &p);
    if ( !s )
        return IO_UNHANDLED;

    if ( !info->dabt.valid )
        return IO_ABORT;

    vio->io_req = p;

    rc = send_ioreq(s, &p, 0);
    if ( rc != IO_RETRY || v->domain->is_shutting_down )
        vio->io_req.state = STATE_IOREQ_NONE;
    else if ( !ioreq_needs_completion(&vio->io_req) )
        rc = IO_HANDLED;
    else
        vio->io_completion = IO_mmio_completion;

    return rc;
}

bool ioreq_complete_mmio(void)
{
    struct vcpu *v = current;
    struct cpu_user_regs *regs = guest_cpu_user_regs();
    const union hsr hsr = { .bits = regs->hsr };
    paddr_t addr = v->io.io_req.addr;

    if ( try_handle_mmio(regs, hsr, addr) == IO_HANDLED )
    {
        advance_pc(regs, hsr);
        return true;
    }

    return false;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
