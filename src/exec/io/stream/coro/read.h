#pragma once

#include "exec/cancel.h"
#include "exec/executor/Executor.h"
#include "exec/io/stream/Stream.h"
#include "exec/os/Service.h"

#include <supp/NonCopyable.h>

#include <coroutine>

namespace exec {

auto read(Stream* stream, char* dst, size_t len) {
    struct Awaitable : Runnable, CancellationHandler {
        Awaitable(Stream* stream, char* dst, size_t len, CancellationSlot slot)
            : stream_{stream}
            , dst_{dst}
            , len_{len}
            , slot_{slot} {}

        bool await_ready() { return performRead(); }

        void await_suspend(std::coroutine_handle<> caller) {
            slot_.installIfConnected(this);
            caller_ = caller;
            service<Executor>()->post(this);
        }

        size_t await_resume() const { return read_; }

        // Runnable
        Runnable* run() override {
            if (performRead()) {
                slot_.clearIfConnected();
                caller_.resume();
            } else {
                // continue polling
                service<Executor>()->post(this);
            }

            return noop;
        }

        // CancellationHandler
        std::coroutine_handle<> cancel() override {
            // next performRead() will return true immediately
            len_ = 0;
            return std::noop_coroutine();
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

    struct Op : supp::NonCopyable {
        Op(Stream* stream, char* dst, size_t len) : stream_{stream}, dst_{dst}, len_{len} {}
        Op(Op&&) = default;

        // CancellableAwaitable
        Op& setCancellationSlot(CancellationSlot slot) {
            slot_ = slot;
            return *this;
        }

        auto operator co_await() { return Awaitable(stream_, dst_, len_, slot_); }

     private:
        Stream* const stream_;
        char* dst_;
        size_t len_;
        CancellationSlot slot_;
    };

    return Op(stream, dst, len);
}

}  // namespace exec
