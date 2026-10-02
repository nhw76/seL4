/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <config.h>
#include <mode/smp/ipi.h>
#include <smp/lock.h>
#include <util.h>
#include <smp/ipi.h>
#ifdef CONFIG_RISCV_AIA
#include <drivers/irq/riscv_aia.h>
#endif

#ifdef ENABLE_SMP_SUPPORT

#ifdef CONFIG_RISCV_AIA
void aia_remote_op(word_t op, irq_t irq, word_t argument)
{
    doRemoteOp3Arg(IpiRemoteCall_Aia, op, irq, argument, 0);
}
#endif

static volatile irq_t ipiIrq[CONFIG_MAX_NUM_NODES];

void handleRemoteCall(IpiRemoteCall_t call, word_t arg0, word_t arg1, word_t arg2, bool_t irqPath)
{
    /* we gets spurious irq_remote_call_ipi calls, e.g. when handling IPI
     * in lock while hardware IPI is pending. Guard against spurious IPIs! */
    if (clh_is_ipi_pending(getCurrentCPUIndex())) {
        switch (call) {
        case IpiRemoteCall_Stall:
            ipiStallCoreCallback(irqPath);
            break;

#ifdef CONFIG_HAVE_FPU
        case IpiRemoteCall_switchFpuOwner:
            switchLocalFpuOwner((tcb_t *)arg0);
            break;
#endif /* CONFIG_HAVE_FPU */

#ifdef CONFIG_RISCV_AIA
        case IpiRemoteCall_Aia:
            assert(getCurrentCPUIndex() == 0);
            switch (arg0) {
            case AIA_REMOTE_ACK: aia_ack_local(arg1); break;
            case AIA_REMOTE_MASK: aia_mask_local(arg2, arg1); break;
            case AIA_REMOTE_TRIGGER: aia_set_trigger_local(arg1, arg2); break;
            default: fail("Invalid AIA remote operation");
            }
            break;
#endif
        default:
            fail("Invalid remote call");
            break;
        }

        big_kernel_lock.node[getCurrentCPUIndex()].ipi = 0;
        ipiIrq[getCurrentCPUIndex()] = irqInvalid;
        ipi_wait();
    }
}

void ipi_send_mask(irq_t ipi, word_t mask, bool_t isBlocking)
{
    generic_ipi_send_mask(ipi, mask, isBlocking);
}

irq_t ipi_get_irq(void)
{
    assert(!(ipiIrq[getCurrentCPUIndex()] == irqInvalid && clh_is_ipi_pending(getCurrentCPUIndex())));
    return ipiIrq[getCurrentCPUIndex()];
}

void ipi_clear_irq(irq_t irq)
{
    ipiIrq[getCurrentCPUIndex()] = irqInvalid;
    return;
}

/* this function is called with a single hart id. */
void ipi_send_target(irq_t irq, word_t hart_id)
{
    word_t hart_mask = BIT(hart_id);
    word_t core_id = hartIDToCoreID(hart_id);
    assert(core_id < CONFIG_MAX_NUM_NODES);

    assert((ipiIrq[core_id] == irqInvalid) || (ipiIrq[core_id] == irq_reschedule_ipi) ||
           (ipiIrq[core_id] == irq_remote_call_ipi && !clh_is_ipi_pending(core_id)));

    ipiIrq[core_id] = irq;
    fence_rw_rw();
    sbi_send_ipi(hart_mask);
}

#endif
