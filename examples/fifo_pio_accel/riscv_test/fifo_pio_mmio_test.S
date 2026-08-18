// Bare-metal SE-mode test for FifoPioAccel (examples/fifo_pio_accel/fifo_pio_accel.sv),
// run against AXION's direct-link AXI4 bridge (RTLPioDevice/Axi4SlaveEngine) via
// examples/fifo_pio_accel/configs/run_fifo_pio.py. Purpose: sanity-check that AXION's
// independently-implemented AXI4 bridge drives the same FIFO DUT logic gem5_cva6's own
// accelerator/fifo_accel.sv is derived from and produces the functionally equivalent
// push/pop/status behavior gem5_cva6's scratch/fifo_test/fifo_test.cc checks there (phases
// 1+2 of that test, adapted to this DUT's simpler register map -- no DMA half here).
//
// Register map (fifo_pio_accel.sv, N=4 default depth):
//   0x00 STATUS    (RO) [0]=empty [1]=full [4:2]=count
//   0x08 FIFO_DATA (RW) write=push, read=pop
// Full-after-4-pushes status = 0x12 (count=100b<<2 | full<<1), empty status = 0x1.
//
// No tohost here -- this is gem5 SE (syscall emulation) mode, not gem5_cva6's bare-metal
// FS-style tohost convention (see run_fifo_pio.py: RiscvTimingSimpleCPU + Process, no
// bbl/link.ld). Reports PASS/FAIL via the real write(2) syscall to fd 1 (gem5 SE passes
// this through to the host's actual stdout) and terminates via exit(2) with 0=pass,
// nonzero=fail reason, both directly usable without any m5ops/tohost plumbing.
//
// gem5 SE's Process loader sets sp to a valid per-Linux-ABI stack region before jumping
// to _start -- deliberately not overridden here, unlike gem5_cva6's bare-metal tests
// which must set up their own stack from scratch.
//
// Build:
//   riscv-none-elf-gcc -march=rv64imafdc -mabi=lp64d -mcmodel=medany -nostartfiles
//     -nostdlib -static -T fifo_pio_mmio_test.ld fifo_pio_mmio_test.S -o fifo_pio_mmio_test.elf

.section .text
.global _start
_start:
    li      t0, 0x10010000      // FIFO_BASE, must match run_fifo_pio.py

    // Phase 1: push 4 distinct values, then check STATUS == full/count=4.
    li      t1, 0xA0
    sd      t1, 8(t0)
    li      t1, 0xA1
    sd      t1, 8(t0)
    li      t1, 0xA2
    sd      t1, 8(t0)
    li      t1, 0xA3
    sd      t1, 8(t0)

    ld      t2, 0(t0)
    li      t3, 0x12
    bne     t2, t3, fail_status_full

    // Phase 2: pop 4 values back in order, check each, then STATUS == empty.
    ld      t2, 8(t0)
    li      t3, 0xA0
    bne     t2, t3, fail_data

    ld      t2, 8(t0)
    li      t3, 0xA1
    bne     t2, t3, fail_data

    ld      t2, 8(t0)
    li      t3, 0xA2
    bne     t2, t3, fail_data

    ld      t2, 8(t0)
    li      t3, 0xA3
    bne     t2, t3, fail_data

    ld      t2, 0(t0)
    li      t3, 0x1
    bne     t2, t3, fail_status_empty

pass:
    la      a1, msg_pass
    li      a2, 51
    j       report

fail_status_full:
    la      a1, msg_fail_status_full
    li      a2, 44
    j       report_and_exit1

fail_data:
    la      a1, msg_fail_data
    li      a2, 45
    j       report_and_exit1

fail_status_empty:
    la      a1, msg_fail_status_empty
    li      a2, 37
    j       report_and_exit1

report:
    li      a0, 1               // fd = stdout
    li      a7, 64              // sys_write
    ecall
    li      a0, 0               // exit code 0 = pass
    li      a7, 93              // sys_exit
    ecall

report_and_exit1:
    li      a0, 1
    li      a7, 64
    ecall
    li      a0, 1               // exit code 1 = fail
    li      a7, 93
    ecall

.section .rodata
msg_pass:               .ascii "PASS: fifo_pio_accel push/pop/status round-trip OK\n"
msg_fail_status_full:   .ascii "FAIL: STATUS after 4 pushes != full/count=4\n"
msg_fail_data:          .ascii "FAIL: popped data did not match pushed order\n"
msg_fail_status_empty:  .ascii "FAIL: STATUS after draining != empty\n"
