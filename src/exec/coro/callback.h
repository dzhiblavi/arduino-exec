#pragma once

#include "exec/coro/traits.h"

namespace exec {

template <typename C, typename T>
concept Callback = requires(C* c, T value) {
    { c->run(value) } -> std::same_as<void>;
};

template <typename S>
concept CallbackService = requires(S& service, typename S::CallbackType* callback) {
    typename S::CallbackArgType;
    typename S::CallbackType;
    requires(Callback<typename S::CallbackType, typename S::CallbackArgType>);
    { service.setCallback(callback) } -> std::same_as<void>;
};

// TODO: add tests?
template <CallbackService Service>
Awaitable auto waitCallback(Service& service) {
    using CallbackType = typename Service::CallbackType;
    using ResultType = Result<typename Service::CallbackArgType>;

    struct Awaiter : CancellationHandler, CallbackType {
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
