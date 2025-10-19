#pragma once

#include "exec/io/int/interrupts.h"

#include "exec/sm/Initiator.h"
#include "exec/sm/Operation.h"

#include "exec/Error.h"
#include "exec/executor/Executor.h"
#include "exec/os/OS.h"

namespace exec {

template <uint8_t IntNo, InterruptMode Mode>
class Interrupt : public Service {
 public:
    Interrupt() {
        if (auto o = os()) {
            o->addService(this);
        }
    }

    Interrupt(const Interrupt&) = delete;
    Interrupt& operator=(const Interrupt&) = delete;

    // Starts tracking interrupts. May be called from ISR.
    void begin() {
        attachInterruptArg(IntNo, isr, this, Mode);
    }

    // Stops tracking interrupts. May be called from ISR.
    // The outstanding operation will NOT be cancelled.
    void end() {
        DASSERT(!op_.outstanding());
        detachInterrupt(IntNo);
    }

    [[nodiscard]] Initiator auto wait(ErrCode* ec) {
        return [this, ec](Runnable* cb, CancellationSlot slot = {}) {
            if (!fired_) {
                op_.initiate(ec, cb, slot);
                return noop;
            }

            fired_ = false;
            *ec = ErrCode::Success;
            return cb;
        };
    }

    void setISR(InterruptFunc func) {
        func_ = func;
    }

    // Service
    void tick() override {
        if (!fired_) {
            return;
        }

        fired_ = false;

        if (op_.outstanding()) {
            executor()->post(op_.complete(ErrCode::Success));
        }
    }

 private:
    static void isr(void* ptr) {
        static_cast<Interrupt*>(ptr)->isr();
    }

    void isr() {
        fired_ = true;

        if (func_ != nullptr) {
            func_();
        }
    }

    volatile bool fired_ = false;
    CancellableOperation<> op_;
    InterruptFunc func_ = nullptr;
};

}  // namespace exec
