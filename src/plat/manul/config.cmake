declare_platform(manul KernelPlatformManul PLAT_MANUL KernelArchRiscV)
if(KernelPlatformManul)
    declare_seL4_arch(riscv64)
    config_set(KernelPlatformFirstHartID FIRST_HART_ID 0)
    config_set(KernelOpenSBIPlatform OPENSBI_PLATFORM "generic")
    list(APPEND KernelDTSList "${CMAKE_CURRENT_LIST_DIR}/manul.dts")
    declare_default_headers(TIMER_FREQUENCY 150000000 MAX_IRQ 0
        INTERRUPT_CONTROLLER drivers/irq/riscv_plic_dummy.h)
endif()
