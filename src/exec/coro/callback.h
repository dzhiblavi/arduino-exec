#pragma once

#include "exec/Callback.h"
#include "exec/coro/traits.h"

namespace exec {

// TODO: add tests?
template <typename Service>
Awaitable auto waitCallback(Service& service) {
    struct Awaiter : CancellationHandler, Callback<typename Service::CallbackArgType> {
        Awaiter(Service& service, CancellationSlot slot) : service_{service}, slot_{slot} {}

        bool await_ready() const { return false; }

        void await_suspend(std::coroutine_handle<> caller) {
            caller_ = caller;
            slot_.installIfConnected(this);
            service_.setCallback(this);
        }

        ErrCode await_resume() {
            slot_.clearIfConnected();
            service_.setCallback(nullptr);
            return code_;
        }

     private:
        // CancellationHandler
        std::coroutine_handle<> cancel() override {
            DASSERT(caller_ != nullptr);
            code_ = ErrCode::Cancelled;
            return caller_;
        }

        // Callback<>
        void run(Service::CallbackArgType /*service*/) override {
            DASSERT(caller_ != nullptr);
            code_ = ErrCode::Success;
            caller_.resume();
        }

        Service& service_;
        CancellationSlot slot_;
        std::coroutine_handle<> caller_ = nullptr;
        ErrCode code_ = ErrCode::Unknown;
    };

    struct Awaitable {
        Awaiter operator co_await() { return Awaiter(service, slot); }

        Service& service;
        CancellationSlot slot{};
    };

    return Awaitable{service};
}

}  // namespace exec
