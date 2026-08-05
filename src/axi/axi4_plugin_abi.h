/*
 * axi4_plugin_abi.h -- the stable C ABI a "plugin" RTL model exposes.
 *
 * Counterpart to axi4_types.hh's C++ Axi4SlavePins/Axi4MasterPins contract,
 * for the opt-in dlopen path (RTLPioDevicePlugin / RTLDmaDevicePlugin, see
 * src/dev/rtl/). Where the default direct-link path binds a leaf class to a
 * specific Verilator-generated top module at compile time, this header lets
 * a .so built from plugin/rtl_plugin.mk (any DUT wired through
 * hw/axi4/axi4_pins.sv, whose flat s_axi_* / m_axi_* pin names are identical
 * across every DUT) be loaded generically at runtime -- no new C++ leaf
 * class needed per model.
 *
 * Plain C, not C++: dlopen crosses a shared-library boundary between two
 * independently compiled binaries, and C++ vtable/ABI layout isn't
 * guaranteed stable across that boundary the way a plain C struct/function
 * ABI is. Struct fields are named field-for-field after axi4_pins.sv's flat
 * port list so the mapping in the shim (plugin/rtl_plugin_shim.cc) and in
 * the dlopen'd gem5 classes is mechanical.
 *
 * Every Set* / Get* pin access on the C++ side (axi4_types.hh) is cached
 * locally by the dlopen'd leaf class and only actually crosses this ABI
 * boundary, batched, inside axiEval() -- one *_drive() + one eval() + one
 * *_sample() per port role per tick, instead of one call per pin.
 */

#ifndef __AXI_AXI4_PLUGIN_ABI_H__
#define __AXI_AXI4_PLUGIN_ABI_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bump on any addition/change; a .so reporting a lower version than the
 * gem5-side loader expects is rejected rather than driven with stale
 * assumptions about struct layout. */
#define AXION_RTL_PLUGIN_ABI_VERSION 1u

/* Opaque handle to one instantiated RTL model (one VerilatedContext + one
 * top-module instance inside the .so). */
typedef struct AxionRtlInstance AxionRtlInstance;

/* -- Slave port (DUT is the AXI4 SLAVE, e.g. a PIO/register port) -------- */

typedef struct
{
    uint32_t awid;
    uint64_t awaddr;
    uint8_t awlen;
    uint8_t awsize;
    uint8_t awburst;
    uint8_t awvalid;

    uint64_t wdata;
    uint64_t wstrb;
    uint8_t wlast;
    uint8_t wvalid;

    uint8_t bready;

    uint32_t arid;
    uint64_t araddr;
    uint8_t arlen;
    uint8_t arsize;
    uint8_t arburst;
    uint8_t arvalid;

    uint8_t rready;
} AxionAxi4SlaveInputs;

typedef struct
{
    uint8_t awready;

    uint8_t wready;

    uint32_t bid;
    uint8_t bresp;
    uint8_t bvalid;

    uint8_t arready;

    uint32_t rid;
    uint64_t rdata;
    uint8_t rresp;
    uint8_t rlast;
    uint8_t rvalid;
} AxionAxi4SlaveOutputs;

/* -- Master port (DUT is the AXI4 MASTER, e.g. a DMA requester) ---------- */

/* Pins the DUT drives as master; sampled by gem5. */
typedef struct
{
    uint32_t awid;
    uint64_t awaddr;
    uint8_t awlen;
    uint8_t awsize;
    uint8_t awburst;
    uint8_t awvalid;

    uint64_t wdata;
    uint64_t wstrb;
    uint8_t wlast;
    uint8_t wvalid;

    uint8_t bready;

    uint32_t arid;
    uint64_t araddr;
    uint8_t arlen;
    uint8_t arsize;
    uint8_t arburst;
    uint8_t arvalid;

    uint8_t rready;
} AxionAxi4MasterOutputs;

/* Pins gem5 drives back into the DUT as master-side responses. */
typedef struct
{
    uint8_t awready;

    uint8_t wready;

    uint32_t bid;
    uint8_t bresp;
    uint8_t bvalid;

    uint8_t arready;

    uint32_t rid;
    uint64_t rdata;
    uint8_t rresp;
    uint8_t rlast;
    uint8_t rvalid;
} AxionAxi4MasterInputs;

/* -- Lifecycle / clock ---------------------------------------------------- */

AxionRtlInstance *axion_rtl_create(const char *instance_name);
void axion_rtl_destroy(AxionRtlInstance *inst);

void axion_rtl_set_clk(AxionRtlInstance *inst, uint8_t val);
void axion_rtl_set_rst_n(AxionRtlInstance *inst, uint8_t val);
/* Settle combinational logic for the current input values, without
 * advancing the clock -- mirrors VerilatedRtlModel<TopT>::settle(). */
void axion_rtl_eval(AxionRtlInstance *inst);

/* Which port roles this .so's TOP module actually implements -- the
 * dlopen'd leaf class fatal()s at construction if the role it needs isn't
 * reported, rather than silently reading zeroed structs. */
int axion_rtl_has_slave_port(void);
int axion_rtl_has_master_port(void);

/* -- Batched pin access ---------------------------------------------------
 * Each pair costs exactly two ABI crossings per tick (plus axion_rtl_eval),
 * regardless of how many individual pins changed. */

void axion_rtl_slave_drive(AxionRtlInstance *inst,
                            const AxionAxi4SlaveInputs *in);
void axion_rtl_slave_sample(AxionRtlInstance *inst,
                             AxionAxi4SlaveOutputs *out);

void axion_rtl_master_drive(AxionRtlInstance *inst,
                             const AxionAxi4MasterInputs *in);
void axion_rtl_master_sample(AxionRtlInstance *inst,
                              AxionAxi4MasterOutputs *out);

unsigned axion_rtl_abi_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* Function-pointer types matching the declarations above, for callers that
 * dlsym() each symbol rather than linking against this ABI directly (e.g.
 * RTLPioDevicePlugin / RTLDmaDevicePlugin, src/dev/rtl/). */
typedef AxionRtlInstance *(*axion_rtl_create_t)(const char *);
typedef void (*axion_rtl_destroy_t)(AxionRtlInstance *);
typedef void (*axion_rtl_set_clk_t)(AxionRtlInstance *, uint8_t);
typedef void (*axion_rtl_set_rst_n_t)(AxionRtlInstance *, uint8_t);
typedef void (*axion_rtl_eval_t)(AxionRtlInstance *);
typedef int (*axion_rtl_has_slave_port_t)(void);
typedef int (*axion_rtl_has_master_port_t)(void);
typedef void (*axion_rtl_slave_drive_t)(AxionRtlInstance *,
                                         const AxionAxi4SlaveInputs *);
typedef void (*axion_rtl_slave_sample_t)(AxionRtlInstance *,
                                          AxionAxi4SlaveOutputs *);
typedef void (*axion_rtl_master_drive_t)(AxionRtlInstance *,
                                          const AxionAxi4MasterInputs *);
typedef void (*axion_rtl_master_sample_t)(AxionRtlInstance *,
                                           AxionAxi4MasterOutputs *);
typedef unsigned (*axion_rtl_abi_version_t)(void);

#endif /* __AXI_AXI4_PLUGIN_ABI_H__ */
