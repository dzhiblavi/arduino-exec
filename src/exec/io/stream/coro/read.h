#pragma once

#include "exec/cancel.h"
#include "exec/executor/Executor.h"
#include "exec/io/stream/Stream.h"
#include "exec/os/Service.h"

#include <logging/log.h>

#include <coroutine>

namespace exec {

auto read(Stream* stream, char* dst, size_t len) {
    struct Awaitable : Runnable, CancellationHandler, supp::Pinned {
        Awaitable(Stream* stream, char* dst, size_t len) : stream_{stream}, dst_{dst}, len_{len} {}

        bool await_ready() noexcept {
            if (!performRead()) {
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
            return read_;
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
            if (performRead()) {
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

            // next performRead() will return true immediately
            len_ = 0;
            return noop;
        }

        bool performRead() {
            while (len_ > 0) {
                int b = stream_->read();
                if (b == -1) {
                    break;
                }

                *(dst_++) = static_cast<char>(b);  // NOLINT
                --len_;
                ++read_;
            }

            return len_ == 0;
        }

        Stream* const stream_;
        char* dst_;
        size_t len_;

        size_t read_ = 0;
        std::coroutine_handle<> caller_;
        CancellationSlot slot_;
    };

    return Awaitable(stream, dst, len);
}

}  // namespace exec
