#!/usr/bin/env bash
# Mirrors AXION's own sources into ext/gem5/src/ so gem5's build system
# picks them up.
#
# gem5's src/SConscript walks with followlinks=False, so a plain
# symlinked directory is invisible to the build -- `cp -rs` instead
# creates *real* directories with each individual *file* symlinked back
# to this repo, which the walk does see. Each mirror target uses a
# basename distinct from its true source dir (rtl_axion / rtl_axion
# examples, not src / examples) since SCons keys build-variant dirs by
# basename and would otherwise collide with gem5's own src/ tree walk.
#
# Run this AFTER `make -C examples/fifo_pio_accel` (or any other example)
# has produced its obj_dir/*.a -- otherwise those generated files won't
# exist yet to be mirrored in. Safe to re-run at any point: it rebuilds
# both mirrors from scratch each time, so deleted/renamed files don't
# linger.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

if [ ! -d ext/gem5/src ]; then
    echo "[AXION] ext/gem5 submodule not checked out -- run scripts/setup_env.sh first" >&2
    exit 1
fi

rm -rf ext/gem5/src/rtl_axion ext/gem5/src/rtl_axion_examples
cp -rs "$(pwd)/src" ext/gem5/src/rtl_axion
cp -rs "$(pwd)/examples" ext/gem5/src/rtl_axion_examples

echo "[AXION] mirrored src/      -> ext/gem5/src/rtl_axion"
echo "[AXION] mirrored examples/ -> ext/gem5/src/rtl_axion_examples"
