#!/usr/bin/env bash
# Mirrors AXION's own sources into ext/gem5/src/ so gem5's build system
# picks them up.
#
# gem5's src/SConscript walks with followlinks=False, so a plain
# symlinked directory is invisible to the build -- `cp -rs` instead
# creates *real* directories with each individual *file* symlinked back
# to this repo, which the walk does see.
#
# Each of our source subdirectories is mirrored at the SAME relative
# path it would occupy inside gem5's own src/ tree (src/axi ->
# ext/gem5/src/axi, src/cpu/rtl -> ext/gem5/src/cpu/rtl, etc.) -- this
# matters because our own code uses gem5-style include paths like
# #include "axi/verilated_model.hh", resolved against gem5's src/ as
# the include root. None of these paths (axi/, cpu/rtl/, dev/rtl/,
# examples/) exist in stock gem5, so there's no collision; matches how
# gem5_cva6 mirrors its own custom sources (gem5/src/cpu/rtl/, not a
# wrapped/prefixed location).
#
# Run this AFTER `make -C examples/fifo_pio_accel` (or any other example)
# has produced its obj_dir/*.a -- otherwise those generated files won't
# exist yet to be mirrored in. Safe to re-run at any point: it rebuilds
# every mirror target from scratch each time, so deleted/renamed files
# don't linger.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

if [ ! -d ext/gem5/src ]; then
    echo "[AXION] ext/gem5 submodule not checked out -- run scripts/setup_env.sh first" >&2
    exit 1
fi

root="$(pwd)"

mirror() {
    local src="$1" dst="ext/gem5/src/$2"
    rm -rf "$dst"
    mkdir -p "$(dirname "$dst")"
    cp -rs "$root/$src" "$dst"
    echo "[AXION] mirrored $src -> $dst"
}

mirror src/axi axi
mirror src/cpu/rtl cpu/rtl
mirror src/dev/rtl dev/rtl
mirror examples examples
