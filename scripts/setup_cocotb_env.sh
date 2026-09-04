#!/usr/bin/env bash
# Creates (or reuses) a Python venv at .venv/ and installs the cocotb-based
# testbenches' dependencies (requirements-cocotb.txt) into it. Independent
# of scripts/setup_env.sh (that one is the gem5 submodule/mirror step) --
# this one has nothing to do with gem5 or Verilator's own build, only the
# Python side of examples/*/cocotb/.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV_DIR="$REPO_ROOT/.venv"

if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR"
fi

"$VENV_DIR/bin/pip" install -q --upgrade pip
"$VENV_DIR/bin/pip" install -q -r "$REPO_ROOT/requirements-cocotb.txt"

echo "cocotb venv ready at $VENV_DIR (activate with: source $VENV_DIR/bin/activate)"
