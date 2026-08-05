#!/usr/bin/env bash
# One-shot environment setup: checks out the gem5 submodule and mirrors
# AXION's own sources into it (see build_gem5.sh). Does NOT verilate any
# example or build gem5 itself -- run `make verilate` then `make gem5`
# at the repo root afterwards (in that order: the mirror step needs the
# verilated .a files to already exist to pick them up).

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

echo "[AXION] checking out ext/gem5 submodule..."
git submodule update --init ext/gem5

echo "[AXION] mirroring src/ into ext/gem5/src/ (pre-verilate pass)..."
scripts/build_gem5.sh

cat <<'EOF'

[AXION] Environment ready. Next steps:
  make verilate   # build examples/fifo_pio_accel's RTL with Verilator
  make gem5       # re-mirror (now including the verilated .a) + scons build
  make run-fifo-example -- --binary <path/to/riscv/elf>
EOF
