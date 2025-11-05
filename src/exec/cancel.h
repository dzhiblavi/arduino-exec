#pragma once

#include <supp/Pinned.h>
#include <supp/verify.h>

#include <logging/log.h>

#include <coroutine>
#include <utility>

namespace exec {

struct CancellationHandler : supp::Pinned {
    virtual ~CancellationHandler() = default;
    [[nodiscard]] virtual std::coroutine_handle<> cancel() = 0;
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

    bool isConnected() const { return handler_ != nullptr; }
    bool hasHandler() const { return handler_ != nullptr && *handler_ != nullptr; }

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

    void install(CancellationHandler* handler) { *handler_ = handler; }
    void clear() { *handler_ = nullptr; }

    CancellationHandler** handler_ = nullptr;

    friend class CancellationSignal;
};

class CancellationSignal : supp::Pinned {
 public:
    void emitSync() { emit().resume(); }

    [[nodiscard]] std::coroutine_handle<> emit() {
        if (!handler_) {
            return std::noop_coroutine();
        }

        // non-reentrant emit
        return std::exchange(handler_, nullptr)->cancel();
    }

    CancellationSlot slot() { return {&handler_}; }
    void clear() { handler_ = nullptr; }
    bool hasHandler() const { return handler_ != nullptr; }

 private:
    CancellationHandler* handler_ = nullptr;
};

}  // namespace exec
