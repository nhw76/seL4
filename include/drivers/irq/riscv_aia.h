/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include <plat/machine/devices_gen.h>

/* Wired interrupts belong to logical core 0; other cores use a synchronous
 * kernel remote call to operate on that core's IMSIC CSRs. */
#if !defined(CONFIG_ARCH_RISCV64)
#error "The AIA backend requires RV64"
#endif
#define HAVE_SET_TRIGGER 1
#define AIA_LAST_SOURCE 31
#define AIA_SYNC_ID 63
#define IMSIC_EIDELIVERY 0x70
#define IMSIC_EITHRESHOLD 0x72
#define IMSIC_EIP0 0x80
#define IMSIC_EIE0 0xc0
#define APLIC_DOMAINCFG 0x0000
#define APLIC_SOURCECFG 0x0004
#define APLIC_SETIPNUM 0x1cdc
#define APLIC_CLRIPNUM 0x1ddc
#define APLIC_SETIENUM 0x1edc
#define APLIC_CLRIENUM 0x1fdc
#define APLIC_GENMSI 0x3000
#define APLIC_TARGET 0x3004
#define APLIC_MSI_MODE BIT(2)
#define APLIC_DELIVERY BIT(8)
#define APLIC_GENMSI_BUSY BIT(12)
#define APLIC_EDGE_HIGH 4
#define APLIC_LEVEL_HIGH 6

extern word_t aia_edge_sources;
void aia_cancel_active_irq(irq_t irq);
bool_t aia_on_owner(void);
enum { AIA_REMOTE_ACK, AIA_REMOTE_MASK, AIA_REMOTE_TRIGGER };
#ifdef ENABLE_SMP_SUPPORT
void aia_remote_op(word_t op, irq_t irq, word_t argument);
#endif

static inline void aia_fence(void)
{
    asm volatile("fence iorw, iorw" ::: "memory");
}
static inline word_t imsic_read(word_t reg)
{
    word_t value;
    asm volatile("csrw 0x150, %1; csrr %0, 0x151" : "=r"(value) : "r"(reg) : "memory");
    return value;
}
static inline void imsic_write(word_t reg, word_t value)
{
    asm volatile("csrw 0x150, %0; csrw 0x151, %1" :: "r"(reg), "r"(value) : "memory");
}
static inline void imsic_set(word_t reg, word_t bits)
{
    asm volatile("csrw 0x150, %0; csrs 0x151, %1" :: "r"(reg), "r"(bits) : "memory");
}
static inline void imsic_clear(word_t reg, word_t bits)
{
    asm volatile("csrw 0x150, %0; csrc 0x151, %1" :: "r"(reg), "r"(bits) : "memory");
}
static inline void aplic_write(word_t offset, uint32_t value)
{
    *(volatile uint32_t *)(APLIC_PPTR + offset) = value;
    aia_fence();
}
static inline uint32_t aplic_read(word_t offset)
{
    uint32_t value = *(volatile uint32_t *)(APLIC_PPTR + offset);
    aia_fence();
    return value;
}

/* AIA 1.0, synchronization with APLIC: the reserved, disabled identity is
 * received after older MSIs to this hart. A fence alone cannot drain MSIs.
 * Executed on the owner hart under the kernel lock or its remote-call protocol.
 */
static inline void aplic_sync(void)
{
    word_t retries = 1000000;
    while (aplic_read(APLIC_GENMSI) & APLIC_GENMSI_BUSY) {
        if (!--retries) { fail("APLIC genmsi busy timeout"); }
    }
    imsic_clear(IMSIC_EIP0, BIT(AIA_SYNC_ID));
    aia_fence();
    aplic_write(APLIC_GENMSI, (CONFIG_FIRST_HART_ID << 18) | AIA_SYNC_ID);
    retries = 1000000;
    while (aplic_read(APLIC_GENMSI) & APLIC_GENMSI_BUSY) {
        if (!--retries) { fail("APLIC genmsi send timeout"); }
    }
    retries = 1000000;
    while (!(imsic_read(IMSIC_EIP0) & BIT(AIA_SYNC_ID))) {
        if (!--retries) { fail("APLIC IMSIC synchronization timeout"); }
    }
    imsic_clear(IMSIC_EIP0, BIT(AIA_SYNC_ID));
}

static inline irq_t irq_backend_claim(void)
{
    word_t top;
    asm volatile("csrrw %0, 0x15c, zero" : "=r"(top) :: "memory");
    word_t id = top >> 16;
    if (id == 0) { return irqInvalid; }
    if (id > AIA_LAST_SOURCE) {
        if (id <= AIA_SYNC_ID) { imsic_clear(IMSIC_EIE0, BIT(id)); }
        return irqInvalid;
    }
    /* Suppress before user notification; retain new EIP arrivals. */
    imsic_clear(IMSIC_EIE0, BIT(id));
    return (irq_t)id;
}
static inline void irq_backend_delivered(irq_t irq)
{
    imsic_clear(IMSIC_EIE0, BIT(irq));
}
static inline void aia_ack_local(irq_t irq)
{
    /* Order device servicing writes before sampling the input at the APLIC. */
    aia_fence();
    if (!(aia_edge_sources & BIT(irq))) {
        aplic_write(APLIC_SETIPNUM, irq);
    }
    imsic_set(IMSIC_EIE0, BIT(irq));
}
static inline void aia_mask_local(bool_t disable, irq_t irq)
{
    if (disable) {
        imsic_clear(IMSIC_EIE0, BIT(irq));
        aplic_write(APLIC_CLRIENUM, irq);
        aplic_write(APLIC_CLRIPNUM, irq);
        aplic_sync();
        imsic_clear(IMSIC_EIP0, BIT(irq));
        aia_cancel_active_irq(irq);
    } else {
        aplic_write(APLIC_SETIENUM, irq);
        aia_ack_local(irq);
    }
}
static inline bool_t irq_backend_trigger_supported(bool_t edge)
{
#ifdef CONFIG_MANUL_QEMU
    return true;
#else
    /* The current FPGA source gateways implement only LEVEL_HIGH. */
    return !edge;
#endif
}
static inline void aia_set_trigger_local(irq_t irq, bool_t edge)
{
    aia_mask_local(true, irq);
    if (edge) { aia_edge_sources |= BIT(irq); }
    else { aia_edge_sources &= ~BIT(irq); }
    aplic_write(APLIC_SOURCECFG + 4 * (irq - 1), edge ? APLIC_EDGE_HIGH : APLIC_LEVEL_HIGH);
    aplic_write(APLIC_TARGET + 4 * (irq - 1), (CONFIG_FIRST_HART_ID << 18) | irq);
}
static inline void irq_backend_ack(irq_t irq)
{
    aia_fence(); /* Order the calling core's device writes before remote rearm. */
#ifdef ENABLE_SMP_SUPPORT
    if (!aia_on_owner()) {
        aia_remote_op(AIA_REMOTE_ACK, irq, 0);
        return;
    }
#endif
    aia_ack_local(irq);
}
static inline void irq_backend_mask(bool_t disable, irq_t irq)
{
    aia_fence();
#ifdef ENABLE_SMP_SUPPORT
    if (!aia_on_owner()) {
        aia_remote_op(AIA_REMOTE_MASK, irq, disable);
        return;
    }
#endif
    aia_mask_local(disable, irq);
}
static inline void irq_backend_set_trigger(irq_t irq, bool_t edge)
{
    aia_fence();
#ifdef ENABLE_SMP_SUPPORT
    if (!aia_on_owner()) {
        aia_remote_op(AIA_REMOTE_TRIGGER, irq, edge);
        return;
    }
#endif
    aia_set_trigger_local(irq, edge);
}
static inline void irq_backend_init_hart(void)
{
    imsic_write(IMSIC_EIDELIVERY, 0);
    imsic_write(IMSIC_EIE0, 0);
    imsic_write(IMSIC_EIP0, 0);
    imsic_write(IMSIC_EITHRESHOLD, 0);
    imsic_write(IMSIC_EIDELIVERY, 1);
}
static inline void irq_backend_init_controller(void)
{
    /* OpenSBI supplies M-domain delegation and locked MSI address routing. */
    aplic_write(APLIC_DOMAINCFG, APLIC_MSI_MODE);
    for (word_t irq = 1; irq <= AIA_LAST_SOURCE; irq++) {
        aplic_write(APLIC_CLRIENUM, irq);
        aplic_write(APLIC_SOURCECFG + 4 * (irq - 1), APLIC_LEVEL_HIGH);
        aplic_write(APLIC_TARGET + 4 * (irq - 1), (CONFIG_FIRST_HART_ID << 18) | irq);
        aplic_write(APLIC_CLRIPNUM, irq);
    }
    aia_edge_sources = 0;
    aplic_sync();
    imsic_write(IMSIC_EIP0, 0);
    aplic_write(APLIC_DOMAINCFG, APLIC_MSI_MODE | APLIC_DELIVERY);
}
