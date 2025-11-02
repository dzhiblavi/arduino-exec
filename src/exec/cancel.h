#pragma once

#include "exec/Runnable.h"

#include <supp/Pinned.h>
#include <supp/verify.h>

#include <logging/log.h>

namespace exec {

struct CancellationHandler : supp::Pinned {
    virtual ~CancellationHandler() = default;
    [[nodiscard]] virtual Runnable* cancel() = 0;
};

class CancellationSlot {
 public:
    CancellationSlot() = default;

    // copyable
    CancellationSlot(const CancellationSlot&) = default;
    CancellationSlot& operator=(const CancellationSlot&) = default;

    // movable
    CancellationSlot(CancellationSlot&& r) noexcept
        : handler_{std::exchange(r.handler_, nullptr)} {}

    bool isConnected() const {
        return handler_ != nullptr;
    }

    bool hasHandler() const {
        return handler_ != nullptr && *handler_ != nullptr;
    }

    void installIfConnected(CancellationHandler* handler) {
        if (isConnected()) {
            install(handler);
        }
    }

    void clearIfConnected() {
        if (isConnected()) {
            clear();
        }
    }

 private:
    CancellationSlot(CancellationHandler** handler) : handler_{handler} {}

    void install(CancellationHandler* handler) {
        *handler_ = handler;
    }

    void clear() {
        *handler_ = nullptr;
    }

    CancellationHandler** handler_ = nullptr;

    friend class CancellationSignal;
};

class CancellationSignal : supp::Pinned {
 public:
    void emit() {
        if (auto* task = emitRaw()) {
            task->runAll();
        }
    }

    [[nodiscard]] Runnable* emitRaw() {
        if (!handler_) {
            return noop;
        }

        return handler_->cancel();
    }

    CancellationSlot slot() {
        return {&handler_};
    }

    void clear() {
        handler_ = nullptr;
    }

    bool hasHandler() {
        return handler_ != nullptr;
    }

 private:
    CancellationHandler* handler_ = nullptr;
};

}  // namespace exec
