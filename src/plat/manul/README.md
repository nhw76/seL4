# Manul PLIC platform

Based on upstream seL4 16.0.0. This branch targets the Manul PS-AXI4
RV64GC/Sv39 platform with four harts (IDs 0–3), a 165 MHz timebase,
and a PLIC at 0x0c000000 with source IDs 1–31 and paired M/S contexts.
It uses the existing SiFive-layout PLIC driver; no AIA backend is included.
Kernel console output and timer programming use SBI. OpenSBI must provide
legacy SBI calls for the upstream kernel's SMP IPI/RFENCE interface.

Select KernelPlatform=manul and supply MANUL_DTS as an absolute path to a
DTS prepared from the custom Manul QEMU machine's generated DTB using
ManulOS scripts/prepare-sel4-dts.py. This converts /memreserve/ entries to
no-map reserved-memory nodes, which seL4 16.0.0 requires. Generate
it with the same RAM and hart count as the intended runtime. The overlay
reserves the first 2 MiB for firmware and keeps the input DTB reservations,
including the top-of-RAM DTB window. The kernel loads at 0x80200000;
ManulOS places its elfloader and embedded images separately at 0x82000000.

The ManulOS repository supplies the custom emulator, OpenSBI, and a shared
kernel/userspace/elfloader build. A four-hart, 1 GiB QEMU smoke test reaches
userspace and validates BootInfo. Hardware boot and the full seL4 regression
suite remain to be validated.
Hardware use requires a matching kernel device-tree input; the original
AIA platform and its older timing/memory assumptions are not used here.
