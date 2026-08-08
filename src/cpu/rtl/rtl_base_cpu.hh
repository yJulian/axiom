/*
 * RTLBaseCpu: a gem5 BaseCPU bridged to a Verilator-simulated RTL core
 * over two full AXI4 master ports (instruction, data), each backed by an
 * Axi4MasterEngine with real ID-ordered out-of-order completion.
 *
 * Tick loop modeled on BaseKvmCPU (src/cpu/kvm/base.hh): run the external
 * engine -- here, the RTL core -- for a slice of cycles, then drain
 * whatever AXI4 transactions it issued, rather than AtomicSimpleCPU's
 * self-contained ISA execution loop (the actual instruction execution
 * happens inside the RTL, not in gem5).
 *
 * Scope note: this ships as a structurally complete, compiling abstract
 * base -- the AXI4 master pin contract and port/tick plumbing are real
 * and match RTLPioDevice/RTLDmaDevice's patterns, but no concrete RTL
 * core is wired up (there is no CPU-core-shaped example RTL to hand, only
 * the FIFO/PIO accelerator). A concrete leaf class wrapping a specific
 * core also owns ThreadContext/ISA/interrupt wiring, exactly as any other
 * BaseCPU subclass does (see RTLBaseCpu.py) -- gem5's checkpointing and
 * interrupt-injection machinery needs a real ThreadContext, and only a
 * concrete core implementation can meaningfully provide one, since the
 * RTL core holds the actual PC/register state, not gem5.
 */

#ifndef __CPU_RTL_RTL_BASE_CPU_HH__
#define __CPU_RTL_RTL_BASE_CPU_HH__

#include <deque>
#include <memory>
#include <unordered_map>

#include "axi/axi4_master_engine.hh"
#include "axi/axi4_types.hh"
#include "cpu/base.hh"
#include "mem/port.hh"
#include "params/RTLBaseCpu.hh"

namespace gem5
{

class RTLBaseCpu : public BaseCPU
{
  protected:
    /**
     * A RequestPort fronting one Axi4MasterEngine. Two instances exist
     * (inst, data); if a leaf's instAxiPins()/dataAxiPins() happen to
     * return the same underlying pin set (a core with one unified AXI4
     * master port), RTLBaseCpu::tick() ticks the data port's engine with
     * driveClock=false so the shared model's registers don't advance
     * twice in one cycle -- see Axi4MasterEngine::tick()'s doc comment.
     */
    class RtlCorePort : public RequestPort, private axion::Axi4MasterEngine::Backend
    {
        RTLBaseCpu &cpu_;
        bool isInst_;
        std::unique_ptr<axion::Axi4MasterEngine> engine_;
        std::unordered_map<uint64_t, PacketPtr> inFlight_;
        std::deque<PacketPtr> retryQueue_;

      public:
        RtlCorePort(const std::string &name, RTLBaseCpu &cpu, bool isInst);

        /** Deferred to init(), once the leaf's pin accessors are safe
         *  to call (virtual dispatch only resolves correctly to the
         *  leaf's overrides after the whole object, leaf included, has
         *  finished constructing). */
        void bindPins(axion::Axi4MasterPins &pins);

        void tick(bool driveClock);

      protected:
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;

        void issueRead(uint64_t seq, Addr addr, unsigned size) override;
        void issueWrite(uint64_t seq, Addr addr, unsigned size,
                         const uint8_t *data) override;
    };

    RtlCorePort instPort_;
    RtlCorePort dataPort_;
    EventFunctionWrapper tickEvent;

    unsigned resetCycles;
    bool resetDone = false;
    unsigned resetCyclesLeft;

    Counter numInsts_ = 0;
    Counter numOps_ = 0;

    void tick();
    void driveResetInputs();

    /**
     * Hook for a leaf CPU to run per-cycle debug/exit logic once both
     * AXI4 ports have ticked for this cycle (e.g. polling core-specific
     * ebreak/illegal-instruction pins and calling exitSimLoop(), or
     * updating numInsts_/numOps_ from a commit-count pin). No-op by
     * default. Only called once reset has completed -- tick() returns
     * early during the reset-hold phase, so postTick() never observes
     * pins that are still held in reset.
     */
    virtual void postTick() {}

  public:
    /**
     * Pin-accessor hooks a concrete leaf must implement, each wrapping a
     * specific Verilated core's instruction-side / data-side AXI4 master
     * pins (they may alias the same object -- see RtlCorePort's doc
     * comment above).
     */
    virtual axion::Axi4MasterPins &instAxiPins() = 0;
    virtual axion::Axi4MasterPins &dataAxiPins() = 0;

    PARAMS(RTLBaseCpu);
    explicit RTLBaseCpu(const Params &p);

    Port &getInstPort() override { return instPort_; }
    Port &getDataPort() override { return dataPort_; }

    void wakeup(ThreadID tid) override;
    Counter totalInsts() const override { return numInsts_; }
    Counter totalOps() const override { return numOps_; }

    void init() override;
    void startup() override;
};

} // namespace gem5

#endif // __CPU_RTL_RTL_BASE_CPU_HH__
