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

    explicit Result(ErrCode code) : code_{code} {  // NOLINT
        DASSERT(!hasValue(), "cannot construct Result<T> with Success code");
    }

    template <typename U>
    requires(!std::same_as<std::remove_reference_t<U>, Result>)
    Result(U&& val) : code_{ErrCode::Success} {  // NOLINT
        new (ptr()) T(std::forward<U>(val));
    }

    Result(Result&& r) : code_{r.code_} {  // NOLINT
        if (!hasValue()) {
            return;
        }

        new (ptr()) T(std::move(r).get());
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
            code_ = r.code_;
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

    Result& operator=(Result&& r) {
        if (this == &r) {
            return *this;
        }

        if (!r.hasValue()) {
            if (hasValue()) {
                release();
            }
            code_ = std::exchange(r.code_, ErrCode::Unknown);
        } else {  // r.hasValue()
            if (hasValue()) {
                get() = std::move(r).get();
            } else {  // !hasValue()
                code_ = ErrCode::Success;
                new (ptr()) T(std::move(r).get());
            }
        }

        return *this;
    }

    ~Result() {
        if (!hasValue()) {
            return;
        }

        release();
    }

    void setError(ErrCode code) {
        DASSERT(code != ErrCode::Success, "cannot emplace Result<T> with Success code");

        if (hasValue()) {
            release();
        }

        code_ = code;
    }

    template <typename U>
    void emplace(U&& val) {
        if (hasValue()) {
            get() = std::forward<U>(val);
        } else {
            code_ = ErrCode::Success;
            new (ptr()) T(std::forward<U>(val));
        }
    }

    bool hasValue() const { return code_ == ErrCode::Success; }

    explicit operator bool() const { return hasValue(); }

    T& operator*() { return get(); }

    const T& operator*() const { return get(); }

    T get() && {
        DASSERT(hasValue());
        auto result = std::move(*ptr());
        release();
        return result;
    }

    T& get() & {
        DASSERT(hasValue());
        return *ptr();
    }

    const T& get() const& {
        DASSERT(hasValue());
        return *ptr();
    }

    ErrCode code() const { return code_; }

 private:
    void release() {
        DASSERT(hasValue());
        ptr()->~T();
        code_ = ErrCode::Unknown;
    }

    T* ptr() { return reinterpret_cast<T*>(data_); }

    const T* ptr() const { return reinterpret_cast<const T*>(data_); }

    alignas(T) uint8_t data_[sizeof(T)] /* uninitialized */;
    ErrCode code_ = ErrCode::Unknown;
};

}  // namespace exec
