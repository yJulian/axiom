/*
 * pcie_tlp_template_test.c -- bare-metal RISC-V guest program for AXION's
 * TLP-level PCIe worked example.
 *
 * Byte for byte the same test as the AXI4-bridge variant's
 * ../../pcie_template_accel/riscv_test/pcie_tlp_template_test.c, against a
 * device with the same register map -- only the expected DeviceID and
 * VERSION differ. That is the point: from software there is no way to tell
 * that this endpoint's RTL parses PCIe transaction-layer packets itself
 * while the other one is handed plain AXI4. Everything below is
 * indifferent to which side of that line the RTL sits on. Does by hand what a BIOS/firmware and then a driver
 * would do, against the RTL-backed endpoint at bus 0, device 1, function 0:
 *
 *   1. read the vendor/device ID out of ECAM config space;
 *   2. program BAR0 and enable memory space in the COMMAND register;
 *   3. talk to the RTL through the BAR window (magic, scratch, counter);
 *   4. burst in and out of the scratchpad RAM behind the BAR;
 *   5. have the device bus-master a DMA copy inside DRAM and verify it;
 *   6. raise an interrupt and watch it arrive as a PLIC pending bit.
 *
 * Steps 1-2 are the part that could not be tested by the cocotb testbench:
 * they exercise gem5's PCI config space and BAR decode (and AXION's
 * RTLPciDevice glue around it), none of which the RTL knows about. Step 3
 * onwards is the same ground the cocotb testbench covers, re-checked here
 * through the whole gem5 stack.
 *
 * Result is reported through `tohost` (1 = pass, 3 = fail), polled by
 * ../configs/run_pcie_tlp_template.py -- the same convention the flexnngine2
 * example and gem5_cva6's bare-metal tests use.
 *
 * Everything is polled; no trap handler is installed, and machine-mode
 * external interrupts are deliberately left masked, so step 6 reads the
 * PLIC's pending bit directly instead of taking the interrupt.
 */

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

/* --- Platform map: gem5's HiFive (ext/gem5/src/dev/riscv/HiFive.py) --- */
#define ECAM_BASE 0x30000000UL /* pci_host.conf_base, conf_device_bits=12 */
#define PCI_MEM_BASE 0x40000000UL /* pci_host.pci_mem_base */
#define PLIC_BASE 0x0C000000UL
#define PLIC_PENDING (PLIC_BASE + 0x1000)

/* Our endpoint's slot; must match pci_dev= in run_pcie_template.py. */
#define PCI_DEV 1
#define ECAM_DEV (ECAM_BASE + (((0 << 8) | (PCI_DEV << 3) | 0) << 12))

/*
 * GenericRiscvPciHost::mapPciInterrupt() routes INTx to PLIC source
 * int_base + (pci_dev % int_count) -- 0x10 + 1 with HiFive's defaults.
 * Note this comes from the *slot*, not from the device's InterruptLine
 * config register.
 */
#define PLIC_SRC (0x10 + PCI_DEV)

/* --- PCI config space offsets --- */
#define PCI_CFG_ID 0x00
#define PCI_CFG_COMMAND 0x04
#define PCI_CFG_BAR0 0x10
#define PCI_CMD_MEM_SPACE (1u << 1)

#define EXPECT_ID 0x00021DE5u /* (DeviceID 0x0002 << 16) | VendorID 0x1DE5 */

/* --- BAR0 register map: see ../pcie_tlp_template_accel.sv --- */
#define REG_ID 0x00
#define REG_VERSION 0x08
#define REG_SCRATCH 0x10
#define REG_COUNTER 0x18
#define REG_DMA_SRC 0x20
#define REG_DMA_DST 0x28
#define REG_DMA_LEN 0x30
#define REG_CTRL 0x38
#define REG_STATUS 0x40
#define REG_IRQ_ACK 0x48
#define BAR_RAM 0x1000

#define CTRL_START (1u << 0)
#define CTRL_IRQ_EN (1u << 1)
#define CTRL_IRQ_FORCE (1u << 2)

#define STATUS_BUSY (1u << 0)
#define STATUS_DONE (1u << 1)
#define STATUS_IRQ_PENDING (1u << 2)

#define ID_MAGIC 0x50434945u /* "PCIE" */
#define ID_VERSION 0x00020000u /* 2 = the TLP-level variant */

/* DMA test buffers, inside the DRAM range and clear of .tohost/_stack_top
 * (see pcie_tlp_template_test.ld). */
#define DMA_SRC 0x80200000UL
#define DMA_DST 0x80300000UL
#define DMA_LEN 64u

#define BAR0 PCI_MEM_BASE /* where step 2 maps the BAR window */

/* Generous but finite: a whole DMA is a few hundred device cycles, and a
 * hang here must fail the test rather than run until the simulator's own
 * timeout. */
#define POLL_LIMIT 200000

volatile uint64_t tohost __attribute__((section(".tohost"), used));

static inline uint32_t
rd32(uint64_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void
wr32(uint64_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

static inline uint64_t
rd64(uint64_t addr)
{
    return *(volatile uint64_t *)addr;
}

static inline void
wr64(uint64_t addr, uint64_t val)
{
    *(volatile uint64_t *)addr = val;
}

/*
 * Park the hart forever once the result is in tohost; the config script's
 * polling loop picks it up and ends the simulation.
 *
 * Deliberately NOT `ebreak`: this is bare metal in M-mode with no trap
 * handler installed, so a breakpoint exception vectors to mtvec == 0 and
 * the very next instruction fetch is an unroutable access to address 0
 * ("Unable to find destination for [0:0x4]"), which looks like a device
 * bug but is just the exit path. A spin loop has no such side effect.
 */
static void
finish(uint64_t code)
{
    tohost = code;
    __asm__ volatile("fence" ::: "memory");
    for (;;)
        __asm__ volatile("nop");
}

/* Every check funnels through here so a failure reports *which* step
 * failed rather than just "failed": tohost carries 3 for step 1, 5 for
 * step 2, ... i.e. (step * 2 + 1), which the config script decodes back
 * into an exit code. 1 alone means success. */
static void
check(int ok, int step)
{
    if (!ok)
        finish((uint64_t)step * 2 + 1);
}

void
pcie_main(void)
{
    /* --- 1. Enumerate: is our endpoint where we expect it? --- */
    check(rd32(ECAM_DEV + PCI_CFG_ID) == EXPECT_ID, 1);

    /* --- 2. Program BAR0 and enable memory space.
     *
     * Writing 0 asks for PCI bus address 0, which GenericPciHost maps to
     * pci_mem_base. Until MEM_SPACE is set in COMMAND the BAR window is
     * not advertised on the bus at all (PciDevice::getAddrRanges() gates
     * on exactly that bit), so the order here matters. --- */
    wr32(ECAM_DEV + PCI_CFG_BAR0, 0);
    check((rd32(ECAM_DEV + PCI_CFG_BAR0) & ~0xFu) == 0, 2);

    wr32(ECAM_DEV + PCI_CFG_COMMAND,
         rd32(ECAM_DEV + PCI_CFG_COMMAND) | PCI_CMD_MEM_SPACE);
    __asm__ volatile("fence" ::: "memory");

    /* --- 3. The RTL is reachable through the BAR window. --- */
    check(rd32(BAR0 + REG_ID) == ID_MAGIC, 3);
    check(rd32(BAR0 + REG_VERSION) == ID_VERSION, 4);

    wr32(BAR0 + REG_SCRATCH, 0xDEADBEEFu);
    check(rd32(BAR0 + REG_SCRATCH) == 0xDEADBEEFu, 5);
    wr32(BAR0 + REG_SCRATCH, 0x0BADF00Du);
    check(rd32(BAR0 + REG_SCRATCH) == 0x0BADF00Du, 6);

    /* The free-running counter must move between two reads -- proof the
     * RTL clock is genuinely advancing and not just being stepped for the
     * duration of each transaction. */
    uint32_t c0 = rd32(BAR0 + REG_COUNTER);
    uint32_t c1 = rd32(BAR0 + REG_COUNTER);
    check(c1 != c0, 7);

    /* --- 4. Scratchpad RAM behind the BAR. --- */
    for (uint64_t i = 0; i < 16; i++)
        wr64(BAR0 + BAR_RAM + i * 8, 0x1000000000000000UL + i);
    for (uint64_t i = 0; i < 16; i++)
        check(rd64(BAR0 + BAR_RAM + i * 8) == 0x1000000000000000UL + i, 8);

    /* --- 5. Bus-master DMA: the device reads and writes DRAM itself. --- */
    for (uint64_t i = 0; i < DMA_LEN / 8; i++) {
        wr64(DMA_SRC + i * 8, 0xA5A5000000000000UL + i);
        wr64(DMA_DST + i * 8, 0);
    }
    __asm__ volatile("fence" ::: "memory");

    wr64(BAR0 + REG_DMA_SRC, DMA_SRC);
    wr64(BAR0 + REG_DMA_DST, DMA_DST);
    wr32(BAR0 + REG_DMA_LEN, DMA_LEN);
    check(rd64(BAR0 + REG_DMA_SRC) == DMA_SRC, 9);

    /* CTRL is one register: OR START into what is already set so the
     * IRQ_EN bit below survives a later start. */
    wr32(BAR0 + REG_CTRL, CTRL_IRQ_EN | CTRL_START);

    uint32_t status = 0;
    int i;
    for (i = 0; i < POLL_LIMIT; i++) {
        status = rd32(BAR0 + REG_STATUS);
        if (status & STATUS_DONE)
            break;
    }
    check(i < POLL_LIMIT, 10);
    check((status & STATUS_BUSY) == 0, 11);

    __asm__ volatile("fence" ::: "memory");
    for (uint64_t j = 0; j < DMA_LEN / 8; j++)
        check(rd64(DMA_DST + j * 8) == 0xA5A5000000000000UL + j, 12);

    /* --- 6. Interrupt: the DMA above already latched IRQ_PENDING with
     * IRQ_EN set, so the device's INTx line should be asserted and the
     * PCI host should have posted it to the PLIC by now. --- */
    check((status & STATUS_IRQ_PENDING) != 0, 13);
    check((rd32(PLIC_PENDING) & (1u << PLIC_SRC)) != 0, 14);

    wr32(BAR0 + REG_IRQ_ACK, 1);
    /* The deassert travels RTL pin -> RTLPciDevice::sampleIrq() ->
     * intrClear() -> PLIC, which takes a few device cycles after the
     * write's own AXI4 response; poll rather than assuming it is
     * immediate. */
    for (i = 0; i < POLL_LIMIT; i++) {
        if ((rd32(PLIC_PENDING) & (1u << PLIC_SRC)) == 0)
            break;
    }
    check(i < POLL_LIMIT, 15);
    check((rd32(BAR0 + REG_STATUS) &
           (STATUS_IRQ_PENDING | STATUS_DONE)) == 0, 16);

    finish(1);
}

__attribute__((section(".text.init"), naked)) void
_start(void)
{
    __asm__ volatile("la sp, _stack_top\n"
                     "call pcie_main\n");
}
