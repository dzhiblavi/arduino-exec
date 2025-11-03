#pragma once

#include "exec/Error.h"

#include <supp/verify.h>

#include <new>  // IWYU pragma: keep
#include <utility>

namespace exec {

template <typename T>
class Result {  // NOLINT
 public:
    Result() = default;

    explicit Result(ErrCode code) noexcept : code_{code} {  // NOLINT
        DASSERT(!hasValue(), "cannot construct Result<T> with Success code");
    }

    template <typename U>
    Result(U&& val) : code_{ErrCode::Success} {  // NOLINT
        new (ptr()) T(std::forward<U>(val));
    }

    Result(const Result& r) : code_{r.code_} {  // NOLINT
        if (!hasValue()) {
            return;
        }

        new (ptr()) T(r.get());
    }

    Result& operator=(const Result& r) {
        if (this == &r) {
            return *this;
        }

        if (!r.hasValue()) {
            if (hasValue()) {
                release();
            }
        } else {  // r.hasValue()
            if (hasValue()) {
                get() = r.get();
            } else {  // !hasValue()
                code_ = ErrCode::Success;
                new (ptr()) T(r.get());
            }
        }

        return *this;
    }

    Result(Result&& r) : code_{r.code_} {  // NOLINT
        if (!hasValue()) {
            return;
        }

        new (ptr()) T(std::move(r).get());
    }

    ~Result() noexcept {
        if (!hasValue()) {
            return;
        }

        release();
    }

    void setError(ErrCode code) noexcept {
        DASSERT(code != ErrCode::Success, "cannot emplace Result<T> with Success code");

        if (hasValue()) {
            release();
        }

        code_ = code;
    }

    template <typename U>
    void emplace(U&& val) noexcept {
        if (hasValue()) {
            get() = std::forward<U>(val);
        } else {
            code_ = ErrCode::Success;
            new (ptr()) T(std::forward<U>(val));
        }
    }

    bool hasValue() const noexcept {
        return code_ == ErrCode::Success;
    }

    explicit operator bool() const noexcept {
        return hasValue();
    }

    T& operator*() noexcept {
        return get();
    }

    const T& operator*() const noexcept {
        return get();
    }

    T get() && noexcept {
        DASSERT(hasValue());
        auto result = std::move(*ptr());
        release();
        return result;
    }

    T& get() & noexcept {
        DASSERT(hasValue());
        return *ptr();
    }

    const T& get() const& noexcept {
        DASSERT(hasValue());
        return *ptr();
    }

    ErrCode code() const noexcept {
        return code_;
    }

 private:
    void release() {
        DASSERT(hasValue());
        ptr()->~T();
        code_ = ErrCode::Unknown;
    }

    T* ptr() noexcept {
        return reinterpret_cast<T*>(data_);
    }

    const T* ptr() const noexcept {
        return reinterpret_cast<const T*>(data_);
    }

    alignas(T) uint8_t data_[sizeof(T)] /* uninitialized */;
    ErrCode code_ = ErrCode::Unknown;
};

}  // namespace exec
