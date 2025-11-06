#pragma once

#include "exec/Callback.h"
#include "exec/coro/traits.h"

namespace exec {

// TODO: add tests?
template <typename Service>
Awaitable auto waitCallback(Service& service) {
    using ResultType = Result<typename Service::CallbackArgType>;

    struct Awaiter : CancellationHandler, Callback<typename Service::CallbackArgType> {
        Awaiter(Service& service, CancellationSlot slot) : service_{service}, slot_{slot} {}

        bool await_ready() const { return false; }

        void await_suspend(std::coroutine_handle<> caller) {
            caller_ = caller;
            slot_.installIfConnected(this);
            service_.setCallback(this);
        }

        ResultType await_resume() {
            slot_.clearIfConnected();
            service_.setCallback(nullptr);
            return std::move(result_);
        }

     private:
        // CancellationHandler
        std::coroutine_handle<> cancel() override {
            DASSERT(caller_ != nullptr);
            result_.setError(ErrCode::Cancelled);
            return caller_;
        }

        // Callback<>
        void run(Service::CallbackArgType value) override {
            DASSERT(caller_ != nullptr);
            result_.emplace(std::forward<typename Service::CallbackArgType>(value));
            caller_.resume();
        }

        Service& service_;
        CancellationSlot slot_;
        std::coroutine_handle<> caller_ = nullptr;
        Result<typename Service::CallbackArgType> result_{};
    };

    struct Awaitable {
        Awaiter operator co_await() { return Awaiter(service, slot); }

        Service& service;
        CancellationSlot slot{};
    };

    return Awaitable{service};
}

}  // namespace exec
