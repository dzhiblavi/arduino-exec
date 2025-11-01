#pragma once

#include "exec/cancel.h"

#include <supp/IntrusiveList.h>
#include <supp/Pinned.h>

#include <coroutine>

namespace exec {

class Mutex;

class [[nodiscard]] LockGuard {
 public:
    LockGuard() = default;
    ~LockGuard();

    LockGuard(LockGuard&& r) noexcept : self_{std::exchange(r.self_, nullptr)} {}
    LockGuard& operator=(LockGuard&& r) noexcept;

    void unlock() &&;

    explicit operator bool() const noexcept {
        return self_ != nullptr;
    }

 private:
    LockGuard(Mutex* self) : self_{self} {}

    Mutex* self_ = nullptr;
    friend class Mutex;
};

class Mutex : supp::Pinned {
 public:
    Mutex() = default;

    LockGuard try_lock() {
        if (tryLockRaw()) {
            return LockGuard{this};
        }

        return LockGuard{nullptr};
    }

    auto lock() {
        return Parked{this};
    }

 private:
    struct [[nodiscard]] Parked : CancellationHandler, supp::IntrusiveListNode, supp::Pinned {
        explicit Parked(Mutex* self) : self_{self} {}

        bool await_ready() noexcept {
            return self_->tryLockRaw();
        }

        // Locked, should park
        void await_suspend(std::coroutine_handle<> caller) {
            slot_.installIfConnected(this);
            caller_ = caller;
            self_->parked_.pushBack(this);
        }

        auto await_resume() const noexcept {
            return LockGuard{self_};
        }

        // CancellableAwaitable
        Parked& setCancellationSlot(CancellationSlot slot) noexcept {
            slot_ = slot;
            return *this;
        }

        // CancellationHandler
        Runnable* cancel() override {
            slot_.clearIfConnected();
            unlink();
            self_ = nullptr;
            caller_.resume();
            return noop;
        }

        void takeLock() {
            slot_.clearIfConnected();
            caller_.resume();
        }

     private:
        Mutex* self_;
        std::coroutine_handle<> caller_;
        CancellationSlot slot_;
    };

    bool tryLockRaw() {
        if (locked_) {
            return false;
        }

        locked_ = true;
        return true;
    }

    void unlock() {
        DASSERT(locked_);

        if (parked_.empty()) {
            locked_ = false;
            return;
        }

        parked_.popFront()->takeLock();
    }

    supp::IntrusiveList<Parked> parked_;
    bool locked_ = false;

    friend class LockGuard;
};

LockGuard::~LockGuard() {
    if (self_ != nullptr) {
        self_->unlock();
    }
}

void LockGuard::unlock() && {
    if (self_ != nullptr) {
        std::exchange(self_, nullptr)->unlock();
    }
}

LockGuard& LockGuard::operator=(LockGuard&& r) noexcept {
    if (this == &r) {
        return *this;
    }

    if (self_ != nullptr) {
        self_->unlock();
    }

    self_ = std::exchange(r.self_, nullptr);
    return *this;
}

}  // namespace exec
