/* pcie_template_linux_ctl.c -- AXION FS-mode Linux control program for
 * PcieTemplateAccel (pcie_template_accel.sv). Covers the same ground as
 * the bare-metal pcie_template_test.c, but from genuine Linux userspace
 * and, crucially, *without* doing any enumeration itself: Linux walks
 * ECAM at boot, sizes and programs BAR0, and exposes the result through
 * sysfs. That is the real proof the endpoint looks like a PCIe device to
 * an operating system, not just to a hand-written probe.
 *
 * Where the bare-metal test writes BAR0 by hand and hardcodes
 * 0x40000000, here the window is whatever Linux assigned -- read via
 * /sys/bus/pci/devices/<slot>/resource0, which mmap()s the BAR directly
 * with no /dev/mem and no root-only physical addressing.
 *
 * Build (statically, for the guest's Linux userspace ABI -- NOT the
 * bare-metal build in this directory's Makefile, which targets
 * pcie_template_test.c):
 *   riscv64-linux-gnu-gcc -static -O1 -o pcie_template_linux_ctl \
 *       pcie_template_linux_ctl.c
 *
 * Then inject it into the disk image at /root/ (see
 * ../configs/run_pcie_template_linux.py's docstring) and run it as root.
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* Slot the config script puts the device in (pci_dev=1, pci_func=0). */
#define PCI_SLOT "0000:00:01.0"
#define SYSFS_DIR "/sys/bus/pci/devices/" PCI_SLOT
#define BAR0_SIZE 0x10000UL

/* BAR0 register map -- see ../pcie_template_accel.sv. Byte offsets. */
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

#define CTRL_IRQ_EN (1u << 1)
#define CTRL_IRQ_FORCE (1u << 2)
#define STATUS_IRQ_PENDING (1u << 2)

#define ID_MAGIC 0x50434945u /* "PCIE" */
#define ID_VERSION 0x00010000u

static volatile unsigned char *bar0;

static uint32_t
rd32(unsigned long off)
{
    return *(volatile uint32_t *)(bar0 + off);
}

static void
wr32(unsigned long off, uint32_t val)
{
    *(volatile uint32_t *)(bar0 + off) = val;
}

static uint64_t
rd64(unsigned long off)
{
    return *(volatile uint64_t *)(bar0 + off);
}

static void
wr64(unsigned long off, uint64_t val)
{
    *(volatile uint64_t *)(bar0 + off) = val;
}

/* Report what the kernel decided about this device, so a failure below
 * can be told apart from "Linux never enumerated it in the first place". */
static void
dump_sysfs(void)
{
    static const char *files[] = {"vendor", "device", "class", "irq"};
    for (unsigned i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char path[256];
        char buf[64];
        snprintf(path, sizeof(path), SYSFS_DIR "/%s", files[i]);
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            printf("  %-8s <unreadable>\n", files[i]);
            continue;
        }
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n <= 0) {
            printf("  %-8s <empty>\n", files[i]);
            continue;
        }
        buf[n] = '\0';
        buf[strcspn(buf, "\n")] = '\0';
        printf("  %-8s %s\n", files[i], buf);
    }
}

int
main(void)
{
    printf("Linux enumerated " PCI_SLOT " as:\n");
    dump_sysfs();

    int fd = open(SYSFS_DIR "/resource0", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open " SYSFS_DIR "/resource0");
        fprintf(stderr, "FAIL: the endpoint was not enumerated by Linux\n");
        return 1;
    }

    void *map = mmap(NULL, BAR0_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                     fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap resource0");
        close(fd);
        return 1;
    }
    bar0 = map;

    uint32_t id = rd32(REG_ID);
    printf("BAR0 ID = 0x%08x, VERSION = 0x%08x\n", id, rd32(REG_VERSION));
    if (id != ID_MAGIC) {
        fprintf(stderr, "FAIL: BAR0 magic 0x%08x != 0x%08x\n", id, ID_MAGIC);
        return 1;
    }
    if (rd32(REG_VERSION) != ID_VERSION) {
        fprintf(stderr, "FAIL: unexpected VERSION\n");
        return 1;
    }

    wr32(REG_SCRATCH, 0xC0FFEE00u);
    if (rd32(REG_SCRATCH) != 0xC0FFEE00u) {
        fprintf(stderr, "FAIL: scratch did not round-trip\n");
        return 1;
    }

    uint32_t c0 = rd32(REG_COUNTER);
    uint32_t c1 = rd32(REG_COUNTER);
    if (c0 == c1) {
        fprintf(stderr, "FAIL: cycle counter is not advancing\n");
        return 1;
    }

    for (unsigned long i = 0; i < 16; i++)
        wr64(BAR_RAM + i * 8, 0x2000000000000000UL + i);
    for (unsigned long i = 0; i < 16; i++) {
        if (rd64(BAR_RAM + i * 8) != 0x2000000000000000UL + i) {
            fprintf(stderr, "FAIL: scratchpad RAM word %lu mismatched\n", i);
            return 1;
        }
    }

    /* DMA is deliberately not exercised here: the device bus-masters to
     * physical addresses, and picking a physically-contiguous, known-safe
     * buffer from userspace needs a kernel driver (dma_alloc_coherent) --
     * out of scope for a template. The bare-metal test covers that path,
     * where the whole physical map is ours to hand out. What is checked
     * here is the interrupt latch, which needs no buffer. */
    wr32(REG_CTRL, CTRL_IRQ_EN | CTRL_IRQ_FORCE);
    if (!(rd32(REG_STATUS) & STATUS_IRQ_PENDING)) {
        fprintf(stderr, "FAIL: forced interrupt did not latch\n");
        return 1;
    }
    wr32(REG_IRQ_ACK, 1);
    if (rd32(REG_STATUS) & STATUS_IRQ_PENDING) {
        fprintf(stderr, "FAIL: IRQ_ACK did not clear the pending bit\n");
        return 1;
    }

    printf("PASS: pcie_template_accel reachable through the BAR Linux "
           "assigned, registers/RAM/interrupt all OK\n");
    munmap(map, BAR0_SIZE);
    close(fd);
    return 0;
}
