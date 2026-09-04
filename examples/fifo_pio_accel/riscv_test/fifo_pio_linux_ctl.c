/* fifo_pio_linux_ctl.c -- AXION FS-mode Linux control program for
 * FifoPioAccel (fifo_pio_accel.sv). Same push/pop/status round-trip as
 * riscv_test/fifo_pio_mmio_test.S (the SE-mode bare-metal test), but run
 * from genuine Linux userspace via /dev/mem + mmap() instead of gem5 SE's
 * Process.map() shortcut -- see run_fifo_pio_linux.py.
 *
 * Register map (fifo_pio_accel.sv, N=4 default depth):
 *   0x00 STATUS    (RO) [0]=empty [1]=full [4:2]=count
 *   0x08 FIFO_DATA (RW) write=push, read=pop
 *
 * Build (statically, for the guest's Linux userspace ABI -- NOT the
 * bare-metal riscv-none-elf-gcc used for fifo_pio_mmio_test.S):
 *   riscv64-linux-gnu-gcc -static -O1 -o fifo_pio_linux_ctl fifo_pio_linux_ctl.c
 *
 * Must run as root in the guest (/dev/mem access).
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define FIFO_BASE 0x10010000UL
#define FIFO_SIZE 0x1000UL
#define REG_STATUS 0
#define REG_DATA 1 /* uint64_t index, i.e. byte offset 0x08 */

int main(void) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        return 1;
    }

    volatile uint64_t *regs = mmap(
        NULL, FIFO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, FIFO_BASE
    );
    if (regs == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    uint64_t status = regs[REG_STATUS];
    printf("initial STATUS = 0x%lx\n", status);
    if (!(status & 0x1)) {
        fprintf(stderr, "FAIL: FIFO not empty at start (STATUS=0x%lx)\n", status);
        return 1;
    }

    uint64_t push_val = 0xC0FFEEUL;
    regs[REG_DATA] = push_val;

    status = regs[REG_STATUS];
    printf("STATUS after push = 0x%lx\n", status);
    if (status & 0x1) {
        fprintf(stderr, "FAIL: FIFO still empty after push\n");
        return 1;
    }

    uint64_t popped = regs[REG_DATA];
    printf("popped = 0x%lx\n", popped);
    if (popped != push_val) {
        fprintf(stderr, "FAIL: popped 0x%lx != pushed 0x%lx\n", popped, push_val);
        return 1;
    }

    status = regs[REG_STATUS];
    if (!(status & 0x1)) {
        fprintf(stderr, "FAIL: FIFO not empty after pop (STATUS=0x%lx)\n", status);
        return 1;
    }

    printf(
        "PASS: fifo_pio_accel push/pop/status round-trip OK via /dev/mem "
        "from Linux userspace\n"
    );
    munmap((void *)regs, FIFO_SIZE);
    close(fd);
    return 0;
}
