#pragma once

#include <exec/executor/Executor.h>

namespace exec::test {

class Executor : public exec::Executor {
 public:
    void post(exec::Runnable* r) override {
        queued.pushBack(r);
    }

    supp::IntrusiveForwardList<exec::Runnable> queued;
};

}  // namespace exec::test
