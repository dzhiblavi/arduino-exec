#pragma once

#include "exec/Error.h"
#include "exec/Unit.h"

#include <supp/verify.h>

#include <concepts>
#include <new>  // IWYU pragma: keep
#include <utility>

namespace exec {

template <typename T>
class Result {  // NOLINT
 public:
    using ValueType = T;

    Result() = default;

    explicit Result(ErrCode code) : code_{code} {  // NOLINT
        DASSERT(!hasValue(), "cannot construct Result<T> with Success code");
    }

    template <typename U>
    requires(!std::same_as<std::remove_reference_t<U>, Result>)
    Result(U&& val) : code_{ErrCode::Success} {  // NOLINT
        new (ptr()) T(std::forward<U>(val));
    }

    Result(Result&& r) noexcept : code_{r.code_} {  // NOLINT
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

    Result& operator=(Result&& r) noexcept {
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
        if (hasValue()) {
            release();
        }
    }

    void setError(ErrCode code) {
        DASSERT(code != ErrCode::Success, "cannot setError with Success code");

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

    T* operator->() { return ptr(); }
    const T* operator->() const { return ptr(); }

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

template <>
class Result<Unit> {  // NOLINT
 public:
    using ValueType = Unit;

    Result() = default;
    Result(Unit) : code_{ErrCode::Success} {}

    explicit Result(ErrCode code) : code_{code} {
        DASSERT(!hasValue(), "cannot construct Result<Unit> with Success code");
    }

    Result(const Result& r) : code_{r.code_} {}
    Result(Result&& r) noexcept : code_{std::exchange(r.code_, ErrCode::Unknown)} {}

    Result& operator=(const Result& r) {
        if (this == &r) {
            return *this;
        }

        code_ = r.code_;
        return *this;
    }

    Result& operator=(Result&& r) {
        if (this == &r) {
            return *this;
        }

        code_ = std::exchange(r.code_, ErrCode::Unknown);
        return *this;
    }

    void setError(ErrCode code) {
        DASSERT(code != ErrCode::Success, "cannot setError with Success code");
        code_ = code;
    }

    void emplace(Unit) { code_ = ErrCode::Success; }

    bool hasValue() const { return code_ == ErrCode::Success; }
    explicit operator bool() const { return hasValue(); }

    Unit operator*() const { return unit; }

    Unit get() && {
        DASSERT(hasValue());
        code_ = ErrCode::Unknown;
        return unit;
    }

    Unit get() const& {
        DASSERT(hasValue());
        return unit;
    }

    ErrCode code() const { return code_; }

 private:
    ErrCode code_ = ErrCode::Unknown;
};

template <typename T>
class Result<T&> {  // NOLINT
 public:
    using ValueType = T&;

    Result() = default;

    explicit Result(ErrCode code) : code_{code} {
        DASSERT(!hasValue(), "cannot construct Result<T&> with Success code");
    }

    template <typename U>
    requires(!std::same_as<std::remove_reference_t<U>, Result>)
    Result(U& val) : ptr_{&val}
                   , code_{ErrCode::Success} {}

    Result(Result&& r) noexcept
        : ptr_{std::exchange(r.ptr_, nullptr)}
        , code_{std::exchange(r.code_, ErrCode::Unknown)} {}

    Result(const Result& r) : ptr_{r.ptr_}, code_{r.code_} {}

    Result& operator=(const Result& r) {
        if (this == &r) {
            return *this;
        }

        code_ = r.code_;
        ptr_ = r.ptr_;
        return *this;
    }

    Result& operator=(Result&& r) noexcept {
        if (this == &r) {
            return *this;
        }

        code_ = std::exchange(r.code_, ErrCode::Unknown);
        ptr_ = std::exchange(r.ptr_, nullptr);
        return *this;
    }

    void setError(ErrCode code) {
        DASSERT(code != ErrCode::Success, "cannot setError with Success code");
        ptr_ = nullptr;
        code_ = code;
    }

    template <typename U>
    void emplace(U& val) {
        ptr_ = &val;
        code_ = ErrCode::Success;
    }

    bool hasValue() const { return code_ == ErrCode::Success; }
    explicit operator bool() const { return hasValue(); }

    T& operator*() { return get(); }
    const T& operator*() const { return get(); }

    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }

    T& get() && {
        DASSERT(hasValue());
        auto ptr = ptr_;
        *this = Result<T&>();
        return *ptr;
    }

    T& get() & {
        DASSERT(hasValue());
        return *ptr_;
    }

    const T& get() const& {
        DASSERT(hasValue());
        return *ptr_;
    }

    ErrCode code() const { return code_; }

 private:
    T* ptr_ = nullptr;
    ErrCode code_ = ErrCode::Unknown;
};

using Status = Result<Unit>;

template <typename T>
Result<T> ok(T value) {
    return Result<T>{std::move(value)};
}

inline Result<Unit> ok() {
    return Result<Unit>{unit};
}

template <typename T = Unit>
Result<T> err(ErrCode code) {
    return Result<T>{code};
}

namespace detail {

template <typename T>
struct IsResult : std::false_type {};

template <typename T>
struct IsResult<Result<T>> : std::true_type {};

}  // namespace detail

template <typename T>
concept IsResult = detail::IsResult<T>::value;

}  // namespace exec
