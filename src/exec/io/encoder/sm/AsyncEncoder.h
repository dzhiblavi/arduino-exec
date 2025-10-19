#pragma once

#include "exec/io/encoder/Encoder.h"

#ifdef EXEC_ARDUINO

#include "exec/sm/Initiator.h"
#include "exec/sm/Operation.h"

namespace exec {

class AsyncEncoder : private Encoder {
 public:
    AsyncEncoder() {
        setCallback(AsyncEncoder::encCallback, this);
    }

    using Encoder::init;
    using Encoder::tick;

    Initiator auto wait(ErrCode* ec) {
        return [&, ec](Runnable* cb, CancellationSlot slot = {}) {
            DASSERT(!op_.outstanding());
            op_.initiate(ec, cb, slot);
            return noop;
        };
    }

    EncButton& impl() {
        return this->impl_;
    }

 protected:
    static void encCallback(void* self) {
        auto* e = static_cast<AsyncEncoder*>(self);

        if (e->op_.outstanding()) {
            e->op_.complete(ErrCode::Success)->runAll();
        }
    }

    CancellableOperation<> op_;
};

}  // namespace exec

#endif
