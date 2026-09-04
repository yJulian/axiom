#!/bin/sh
# Auto-run after Linux boot via gem5's `m5 readfile` mechanism (see
# run_fifo_pio_linux.py --readfile-contents). Exercises FifoPioAccel from
# genuine Linux userspace and reports the result on the simulated console.
echo "=== AXION FS-mode FIFO accel test ==="
/root/fifo_pio_linux_ctl
echo "=== exit code: $? ==="
