#!/bin/sh
# Auto-run after Linux boot via gem5's `m5 readfile` mechanism (see
# run_pcie_template_linux.py --readfile-contents). Shows that the kernel
# enumerated the endpoint on its own, then exercises it from userspace
# through the BAR Linux assigned.
echo "=== AXION FS-mode PCIe template test ==="
lspci -nn 2>/dev/null || echo "(no lspci in image; see sysfs dump below)"
/root/pcie_template_linux_ctl
echo "=== exit code: $? ==="
