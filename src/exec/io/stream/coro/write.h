#pragma once

#include "exec/cancel.h"
#include "exec/executor/Executor.h"
#include "exec/io/stream/Stream.h"
#include "exec/os/Service.h"

#include <supp/NonCopyable.h>

#include <algorithm>
#include <coroutine>

namespace exec {

auto write(Print* print, const char* dst, size_t len) {
    struct Awaitable : Runnable, CancellationHandler {
        Awaitable(Print* print, const char* buf, size_t len, CancellationSlot slot)
            : print_{print}
            , buf_{buf}
            , len_{len}
            , slot_{slot} {}

        bool await_ready() {
            return performWrite();
        }

        void await_suspend(std::coroutine_handle<> caller) {
            slot_.installIfConnected(this);
            caller_ = caller;
            service<Executor>()->post(this);
        }

        size_t await_resume() const {
            return wrote_;
        }

     private:
        // Runnable
        Runnable* run() override {
            if (performWrite()) {
                slot_.clearIfConnected();
                caller_.resume();
            } else {
                // continue polling
                service<Executor>()->post(this);
            }

            return noop;
        }

        // CancellationHandler
        Runnable* cancel() override {
            // next performWrite() will return true immediately
            len_ = 0;
            return noop;
        }

        bool performWrite() {
            while (len_ > 0) {
                int available = print_->availableForWrite();
                if (available <= 0) {
                    return false;
                }

                int write = std::min(available, static_cast<int>(len_));
                write = static_cast<int>(print_->write(buf_, write));

                len_ -= write;
                wrote_ += write;
                buf_ += write;  // NOLINT
            }

            return len_ == 0;
        }

        Print* const print_;
        const char* buf_;
        size_t len_;
        CancellationSlot slot_;

        size_t wrote_ = 0;
        std::coroutine_handle<> caller_;
    };

    struct Op {
        // CancellableAwaitable
        Op& setCancellationSlot(CancellationSlot slot) {
            this->slot = slot;
            return *this;
        }

        auto operator co_await() {
            return Awaitable{print, buf, len, slot};
        }

        Print* const print;
        const char* buf;
        size_t len;
        CancellationSlot slot{};
    };

    return Op{print, dst, len};
}

}  // namespace exec
