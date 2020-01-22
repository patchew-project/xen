/*
 * Architecture-specific vm_event handling routines
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include <xen/sched.h>
#include <asm/vm_event.h>

void vm_event_fill_regs(vm_event_request_t *req)
{
    /* TODO */
}

void vm_event_set_registers(struct vcpu *v, vm_event_response_t *rsp)
{
    /* TODO */
}

void vm_event_monitor_next_interrupt(struct vcpu *v)
{
    /* Not supported on ARM. */
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
