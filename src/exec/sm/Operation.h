#pragma once

#include "exec/Error.h"
#include "exec/cancel.h"

#include <stdlike/type_traits.h>

namespace exec {

class Operation {
 public:
    void initiate(ErrCode* ec, Runnable* cb, CancellationSlot slot, CancellationHandler* handler) {
        DASSERT(!outstanding());
        cb_ = cb;
        ec_ = ec;
        *ec_ = ErrCode::Unknown;
        slot_ = slot;
        slot_.installIfConnected(handler);
    }

    [[nodiscard]] Runnable* complete(ErrCode code) {
        DASSERT(outstanding());
        detachCancellation();
        *ec_ = code;
        return stdlike::exchange(cb_, nullptr);
    }

    void detachCancellation() {
        slot_.clearIfConnected();
    }

    bool outstanding() const {
        return cb_ != nullptr;
    }

 private:
    CancellationSlot slot_;
    ErrCode* ec_ = nullptr;
    Runnable* cb_ = noop;
};

template <typename Self = void>
class CancellableOperation : public Operation, CancellationHandler {
 public:
    void initiate(ErrCode* ec, Runnable* cb, CancellationSlot slot) {
        Operation::initiate(ec, cb, slot, this);
    }

    [[nodiscard]] Runnable* cancel() override {
        DASSERT(slot_.hasHandler(), "cancel() called when not installed");
        executeCancelHook();
        return complete(ErrCode::Cancelled);
    }

 private:
    void executeCancelHook() {
        if constexpr (!stdlike::same_as<Self, void>) {
            static_cast<Self*>(this)->cancelHook();
        }
    }
};

}  // namespace exec
