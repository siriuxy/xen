/*
 * arch/x86/monitor.c
 *
 * Architecture-specific monitor_op domctl handler.
 *
 * Copyright (c) 2015 Tamas K Lengyel (tamas@tklengyel.com)
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <xen/config.h>
#include <xen/sched.h>
#include <xen/mm.h>
#include <asm/domain.h>
#include <asm/monitor.h>
#include <public/domctl.h>
#include <xsm/xsm.h>

/*
 * Sanity check whether option is already enabled/disabled
 */
static inline
int status_check(struct xen_domctl_monitor_op *mop, bool_t status)
{
    bool_t requested_status = (mop->op == XEN_DOMCTL_MONITOR_OP_ENABLE);

    if ( status == requested_status )
        return -EEXIST;

    return 0;
}

int monitor_domctl(struct domain *d, struct xen_domctl_monitor_op *mop)
{
    int rc;
    struct arch_domain *ad = &d->arch;

    rc = xsm_vm_event_control(XSM_PRIV, d, mop->op, mop->event);
    if ( rc )
        return rc;

    /*
     * At the moment only Intel HVM domains are supported. However, event
     * delivery could be extended to AMD and PV domains.
     */
    if ( !is_hvm_domain(d) || !cpu_has_vmx )
        return -EOPNOTSUPP;

    /*
     * Sanity check
     */
    if ( mop->op != XEN_DOMCTL_MONITOR_OP_ENABLE &&
         mop->op != XEN_DOMCTL_MONITOR_OP_DISABLE )
        return -EOPNOTSUPP;

    switch ( mop->event )
    {
    case XEN_DOMCTL_MONITOR_EVENT_MOV_TO_CR0:
    {
        bool_t status = ad->monitor.mov_to_cr0_enabled;

        rc = status_check(mop, status);
        if ( rc )
            return rc;

        ad->monitor.mov_to_cr0_sync = mop->u.mov_to_cr.sync;
        ad->monitor.mov_to_cr0_onchangeonly = mop->u.mov_to_cr.onchangeonly;

        domain_pause(d);
        ad->monitor.mov_to_cr0_enabled = !status;
        domain_unpause(d);
        break;
    }

    case XEN_DOMCTL_MONITOR_EVENT_MOV_TO_CR3:
    {
        bool_t status = ad->monitor.mov_to_cr3_enabled;
        struct vcpu *v;

        rc = status_check(mop, status);
        if ( rc )
            return rc;

        ad->monitor.mov_to_cr3_sync = mop->u.mov_to_cr.sync;
        ad->monitor.mov_to_cr3_onchangeonly = mop->u.mov_to_cr.onchangeonly;

        domain_pause(d);
        ad->monitor.mov_to_cr3_enabled = !status;
        domain_unpause(d);

        /* Latches new CR3 mask through CR0 code */
        for_each_vcpu ( d, v )
            hvm_funcs.update_guest_cr(v, 0);
        break;
    }

    case XEN_DOMCTL_MONITOR_EVENT_MOV_TO_CR4:
    {
        bool_t status = ad->monitor.mov_to_cr4_enabled;

        rc = status_check(mop, status);
        if ( rc )
            return rc;

        ad->monitor.mov_to_cr4_sync = mop->u.mov_to_cr.sync;
        ad->monitor.mov_to_cr4_onchangeonly = mop->u.mov_to_cr.onchangeonly;

        domain_pause(d);
        ad->monitor.mov_to_cr4_enabled = !status;
        domain_unpause(d);
        break;
    }

    case XEN_DOMCTL_MONITOR_EVENT_MOV_TO_MSR:
    {
        bool_t status = ad->monitor.mov_to_msr_enabled;

        rc = status_check(mop, status);
        if ( rc )
            return rc;

        if ( mop->op == XEN_DOMCTL_MONITOR_OP_ENABLE &&
             mop->u.mov_to_msr.extended_capture )
        {
            if ( hvm_funcs.enable_msr_exit_interception )
            {
                ad->monitor.mov_to_msr_extended = 1;
                hvm_funcs.enable_msr_exit_interception(d);
            }
            else
                return -EOPNOTSUPP;
        } else
            ad->monitor.mov_to_msr_extended = 0;

        domain_pause(d);
        ad->monitor.mov_to_msr_enabled = !status;
        domain_unpause(d);
        break;
    }

    case XEN_DOMCTL_MONITOR_EVENT_SINGLESTEP:
    {
        bool_t status = ad->monitor.singlestep_enabled;

        rc = status_check(mop, status);
        if ( rc )
            return rc;

        domain_pause(d);
        ad->monitor.singlestep_enabled = !status;
        domain_unpause(d);
        break;
    }

    case XEN_DOMCTL_MONITOR_EVENT_SOFTWARE_BREAKPOINT:
    {
        bool_t status = ad->monitor.software_breakpoint_enabled;

        rc = status_check(mop, status);
        if ( rc )
            return rc;

        domain_pause(d);
        ad->monitor.software_breakpoint_enabled = !status;
        domain_unpause(d);
        break;
    }

    default:
        return -EOPNOTSUPP;

    };

    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */