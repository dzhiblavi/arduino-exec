#pragma once

#include "exec/cancel.h"
#include "exec/executor/Executor.h"
#include "exec/io/stream/Stream.h"
#include "exec/os/Service.h"

#include <logging/log.h>

#include <algorithm>
#include <coroutine>

namespace exec {

auto write(Print* print, const char* dst, size_t len) {
    struct Awaitable : Runnable, CancellationHandler, supp::Pinned {
        Awaitable(Print* print, const char* buf, size_t len)
            : print_{print}
            , buf_{buf}
            , len_{len} {}

        bool await_ready() noexcept {
            if (!performWrite()) {
                return false;
            }

            slot_.clearIfConnected();
            return true;
        }

        void await_suspend(std::coroutine_handle<> caller) noexcept {
            caller_ = caller;
            service<Executor>()->post(this);
        }

        size_t await_resume() const noexcept {
            return wrote_;
        }

        // CancellableAwaitable
        Awaitable& setCancellationSlot(CancellationSlot slot) noexcept {
            slot_ = slot;
            slot_.installIfConnected(this);
            return *this;
        }

     private:
        // Runnable
        Runnable* run() override {
            if (performWrite()) {
                caller_.resume();
            } else {
                // continue polling
                service<Executor>()->post(this);
            }

            return noop;
        }

        // CancellationHandler
        Runnable* cancel() override {
            slot_.clearIfConnected();

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

        size_t wrote_ = 0;
        std::coroutine_handle<> caller_;
        CancellationSlot slot_;
    };

    return Awaitable(print, dst, len);
}

}  // namespace exec
