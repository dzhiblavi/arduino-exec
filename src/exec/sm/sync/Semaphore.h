#pragma once

#include "exec/executor/global.h"
#include "exec/sm/Initiator.h"

#include <supp/IntrusiveForwardList.h>
#include <supp/Pinned.h>

namespace exec {

class Semaphore : supp::Pinned {
 public:
    explicit Semaphore(int init) : counter_{init} {}

    bool tryAcquire() {
        if (counter_ == 0) {
            return false;
        }

        --counter_;
        return true;
    }

    [[nodiscard]] Initiator auto acquire() {
        return [this](Runnable* cb, CancellationSlot /*ignored*/ = {}) {
            if (counter_ > 0) {
                --counter_;
                return cb;
            }

            queue_.pushBack(cb);
            return noop;
        };
    }

    void release() {
        if (counter_++ != 0 || queue_.empty()) {
            return;
        }

        --counter_;
        executor()->post(queue_.popFront());
    }

 private:
    supp::IntrusiveForwardList<Runnable> queue_;
    int counter_ = 0;
};

}  // namespace exec
