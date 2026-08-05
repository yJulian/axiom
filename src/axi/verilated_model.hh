/*
 * VerilatedRtlModel<TopT>: thin RAII wrapper around a Verilator-generated
 * top module. No abstract-interface/dlopen indirection here on purpose --
 * AXION links its RTL directly (unlike e.g. gem5_cva6's AccelInterface,
 * which exists there to support swapping RTL backends at runtime; that
 * isn't a goal for this scaffold, so the leaf SimObject just
 * #includes its Vxxx_top.h directly and owns one of these).
 */

#ifndef __AXI_VERILATED_MODEL_HH__
#define __AXI_VERILATED_MODEL_HH__

#include <memory>
#include <string>

#include <verilated.h>
#if VM_TRACE
#include <verilated_fst_c.h>
#endif

namespace gem5
{
namespace axion
{

template <class TopT>
class VerilatedRtlModel
{
  public:
    explicit VerilatedRtlModel(const std::string &instanceName)
        : context_(new VerilatedContext),
          top_(new TopT(context_.get(), instanceName.c_str()))
    {}

    ~VerilatedRtlModel()
    {
        top_->final();
#if VM_TRACE
        closeTrace();
#endif
    }

    TopT *top() { return top_.get(); }

    /** Classic Verilator double-eval clock toggle: one full cycle. */
    void clockEdge()
    {
        top_->eval();
    }

    /** Single eval to let combinational logic re-settle after driving
     *  new inputs, without advancing the clock. */
    void settle() { top_->eval(); }

#if VM_TRACE
    void
    openTrace(const std::string &path)
    {
        context_->traceEverOn(true);
        trace_.reset(new VerilatedFstC);
        top_->trace(trace_.get(), 99);
        trace_->open(path.c_str());
    }

    void
    dumpTrace(uint64_t time)
    {
        if (trace_)
            trace_->dump(time);
    }

    void
    closeTrace()
    {
        if (trace_) {
            trace_->close();
            trace_.reset();
        }
    }
#endif

  private:
    std::unique_ptr<VerilatedContext> context_;
    std::unique_ptr<TopT> top_;
#if VM_TRACE
    std::unique_ptr<VerilatedFstC> trace_;
#endif
};

} // namespace axion
} // namespace gem5

#endif // __AXI_VERILATED_MODEL_HH__
