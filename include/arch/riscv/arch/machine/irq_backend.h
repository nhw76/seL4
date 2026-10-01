/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

/* Called with local interrupts disabled. The platform driver is included first.
 * Dispatch completion is separate from userspace service completion.
 */
#ifndef CONFIG_RISCV_AIA
static inline irq_t irq_backend_claim(void)
{
    irq_t irq = plic_get_claim();
#ifdef CONFIG_PLAT_QEMU_RISCV_VIRT
    /* Preserve the existing QEMU PLIC workaround. */
    plic_complete_claim(irq);
#endif
    return irq;
}
static inline void irq_backend_mask(bool_t disable, irq_t irq)
{
    plic_mask_irq(disable, irq);
}
static inline void irq_backend_delivered(irq_t irq)
{
    /* PLIC claim suppresses forwarding until userspace completes it. */
}
static inline void irq_backend_ack(irq_t irq)
{
#ifndef CONFIG_PLAT_QEMU_RISCV_VIRT
    plic_complete_claim(irq);
#endif
}
#ifdef HAVE_SET_TRIGGER
static inline bool_t irq_backend_trigger_supported(bool_t edge) { return true; }
static inline void irq_backend_set_trigger(irq_t irq, bool_t edge)
{
    plic_irq_set_trigger(irq, edge);
}
#endif
static inline void irq_backend_init_hart(void) { plic_init_hart(); }
static inline void irq_backend_init_controller(void) { plic_init_controller(); }
#endif
