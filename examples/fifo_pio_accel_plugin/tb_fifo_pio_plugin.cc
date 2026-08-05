// tb_fifo_pio_plugin.cc
//
// Plugin-path analog of examples/fifo_pio_accel/tb_fifo_pio.cc: exercises
// the exact same AXI4 slave protocol (AW/W/B write, AR/R read) and the
// exact same push/pop round-trip against the exact same DUT
// (fifo_pio_accel.sv, wired through fifo_pio_top.sv) -- but drives it
// purely through the .so built by `make -C examples/fifo_pio_accel_plugin
// plugin-so` (via plugin/rtl_plugin.mk), dlopen()'d at runtime through
// src/axi/axi4_plugin_abi.h alone. No Vfifo_pio_top.h, no Verilator
// headers, no compile-time knowledge of the DUT at all -- this is the
// concrete proof the plugin ABI is genuinely usable generically.
//
// Usage: tb_fifo_pio_plugin <path-to-.so>

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <dlfcn.h>

#include "axi/axi4_plugin_abi.h"

namespace
{

struct Abi
{
    axion_rtl_create_t create;
    axion_rtl_destroy_t destroy;
    axion_rtl_set_clk_t setClk;
    axion_rtl_set_rst_n_t setRstN;
    axion_rtl_eval_t eval;
    axion_rtl_has_slave_port_t hasSlavePort;
    axion_rtl_slave_drive_t slaveDrive;
    axion_rtl_slave_sample_t slaveSample;
    axion_rtl_abi_version_t abiVersion;
};

void *
mustDlsym(void *handle, const char *name)
{
    void *sym = dlsym(handle, name);
    if (!sym) {
        std::fprintf(stderr, "missing symbol '%s': %s\n", name, dlerror());
        std::exit(1);
    }
    return sym;
}

Abi
loadAbi(void *handle)
{
    Abi abi;
    abi.create = reinterpret_cast<axion_rtl_create_t>(
        mustDlsym(handle, "axion_rtl_create"));
    abi.destroy = reinterpret_cast<axion_rtl_destroy_t>(
        mustDlsym(handle, "axion_rtl_destroy"));
    abi.setClk = reinterpret_cast<axion_rtl_set_clk_t>(
        mustDlsym(handle, "axion_rtl_set_clk"));
    abi.setRstN = reinterpret_cast<axion_rtl_set_rst_n_t>(
        mustDlsym(handle, "axion_rtl_set_rst_n"));
    abi.eval = reinterpret_cast<axion_rtl_eval_t>(
        mustDlsym(handle, "axion_rtl_eval"));
    abi.hasSlavePort = reinterpret_cast<axion_rtl_has_slave_port_t>(
        mustDlsym(handle, "axion_rtl_has_slave_port"));
    abi.slaveDrive = reinterpret_cast<axion_rtl_slave_drive_t>(
        mustDlsym(handle, "axion_rtl_slave_drive"));
    abi.slaveSample = reinterpret_cast<axion_rtl_slave_sample_t>(
        mustDlsym(handle, "axion_rtl_slave_sample"));
    abi.abiVersion = reinterpret_cast<axion_rtl_abi_version_t>(
        mustDlsym(handle, "axion_rtl_abi_version"));
    return abi;
}

// One full AXI4 clock cycle: drive the current input struct, toggle
// clk 0->1->0 (matching Axi4SlaveEngine::tick()'s protocol pattern, see
// CLAUDE.md's "Per-cycle protocol pattern"), then sample outputs.
void
tick(const Abi &abi, AxionRtlInstance *inst, const AxionAxi4SlaveInputs &in,
     AxionAxi4SlaveOutputs *out)
{
    abi.slaveDrive(inst, &in);
    abi.setClk(inst, 0);
    abi.eval(inst);
    abi.setClk(inst, 1);
    abi.eval(inst);
    abi.setClk(inst, 0);
    abi.eval(inst);
    abi.slaveSample(inst, out);
}

// `in`/`out` persist across calls (mirroring tb_fifo_pio.cc's single
// persistent `top` object): a *ready pin can legitimately drop the very
// cycle a request is accepted (the slave going briefly "busy"), so
// readiness must be checked against the sample from *before* this
// section starts driving its valid signal, not re-polled after each new
// edge -- otherwise a legitimately-busy-after-accepting slave looks
// indistinguishable from "still hasn't accepted" and the wait never ends.
// The pattern below is exactly tb_fifo_pio.cc's: skip the wait loop if
// already ready, then always take exactly one committing edge.

// Drives one full single-beat AXI4 write burst (AW -> W -> B).
void
axiWrite(const Abi &abi, AxionRtlInstance *inst, AxionAxi4SlaveInputs &in,
         AxionAxi4SlaveOutputs &out, uint64_t addr, uint64_t data,
         unsigned id)
{
    in.awid = id;
    in.awaddr = addr;
    in.awlen = 0;
    in.awsize = 3;
    in.awburst = 1; // INCR
    in.awvalid = 1;
    while (!out.awready)
        tick(abi, inst, in, &out);
    tick(abi, inst, in, &out); // handshake edge
    in.awvalid = 0;

    in.wdata = data;
    in.wstrb = 0xff;
    in.wlast = 1;
    in.wvalid = 1;
    while (!out.wready)
        tick(abi, inst, in, &out);
    tick(abi, inst, in, &out); // handshake edge
    in.wvalid = 0;

    in.bready = 1;
    while (!out.bvalid)
        tick(abi, inst, in, &out);
    assert(out.bresp == 0 && "expected AXI_RESP_OKAY on write");
    tick(abi, inst, in, &out); // handshake edge
    in.bready = 0;
}

// Drives one full single-beat AXI4 read burst (AR -> R) and returns the
// data beat.
uint64_t
axiRead(const Abi &abi, AxionRtlInstance *inst, AxionAxi4SlaveInputs &in,
        AxionAxi4SlaveOutputs &out, uint64_t addr, unsigned id)
{
    in.arid = id;
    in.araddr = addr;
    in.arlen = 0;
    in.arsize = 3;
    in.arburst = 1; // INCR
    in.arvalid = 1;
    while (!out.arready)
        tick(abi, inst, in, &out);
    tick(abi, inst, in, &out); // handshake edge
    in.arvalid = 0;

    in.rready = 1;
    while (!out.rvalid)
        tick(abi, inst, in, &out);
    assert(out.rresp == 0 && "expected AXI_RESP_OKAY on read");
    uint64_t data = out.rdata;
    tick(abi, inst, in, &out); // handshake edge
    in.rready = 0;
    return data;
}

} // namespace

int
main(int argc, char **argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <path-to-.so>\n", argv[0]);
        return 1;
    }

    void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::fprintf(stderr, "dlopen('%s') failed: %s\n", argv[1],
                      dlerror());
        return 1;
    }

    Abi abi = loadAbi(handle);
    if (abi.abiVersion() != AXION_RTL_PLUGIN_ABI_VERSION) {
        std::fprintf(stderr, "unexpected plugin ABI version %u (expected %u)\n",
                      abi.abiVersion(), AXION_RTL_PLUGIN_ABI_VERSION);
        return 1;
    }
    if (!abi.hasSlavePort()) {
        std::fprintf(stderr, "'%s' does not implement an AXI4 slave port\n",
                      argv[1]);
        return 1;
    }

    AxionRtlInstance *inst = abi.create("tb_fifo_pio_plugin");

    AxionAxi4SlaveInputs in{};
    AxionAxi4SlaveOutputs out{};

    abi.setRstN(inst, 0);
    for (int i = 0; i < 10; i++)
        tick(abi, inst, in, &out);
    abi.setRstN(inst, 1);
    tick(abi, inst, in, &out);

    // STATUS (0x00) should read back empty=1, full=0, count=0.
    uint64_t status = axiRead(abi, inst, in, out, 0x00, /*id=*/0);
    assert((status & 0x1) == 1 && "expected empty on reset");
    assert(((status >> 1) & 0x1) == 0 && "expected not-full on reset");

    // Push one value over FIFO_DATA (0x08), then pop it back and check it
    // round-trips. Distinct AWID/ARID exercise the ID field itself
    // (echoed back on B/R), not just the data path.
    const uint64_t pushed = 0xdeadbeefcafef00dULL;
    axiWrite(abi, inst, in, out, 0x08, pushed, /*id=*/5);

    status = axiRead(abi, inst, in, out, 0x00, /*id=*/1);
    assert((status & 0x1) == 0 && "expected not-empty after push");

    uint64_t popped = axiRead(abi, inst, in, out, 0x08, /*id=*/7);
    assert(popped == pushed && "popped value must match what was pushed");

    status = axiRead(abi, inst, in, out, 0x00, /*id=*/2);
    assert((status & 0x1) == 1 && "expected empty again after pop");

    abi.destroy(inst);
    dlclose(handle);

    std::printf(
        "PASS: fifo_pio_top AXI4 push/pop round-trip via plugin ABI "
        "(0x%016llx)\n",
        static_cast<unsigned long long>(pushed));
    return 0;
}
