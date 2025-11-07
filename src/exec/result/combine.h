#pragma once

#include "exec/result/traits.h"

#include <supp/NonCopyable.h>

#include <type_traits>
#include <utility>

namespace exec {

namespace pipe {

template <typename F>
struct [[nodiscard]] AndThen : supp::NonCopyable {
    F user;

    explicit AndThen(F u) : user(std::move(u)) {}

    template <typename T>
    using U = traits::ValueOf<std::invoke_result_t<F, T>>;

    template <typename T>
    Result<U<T>> pipe(Result<T> r) {
        if (r.hasValue()) {
            return user(std::move(r).get());
        }

        return err<U<T>>(r.code());
    }
};

template <typename F>
struct [[nodiscard]] OrElse : supp::NonCopyable {
    F user;

    explicit OrElse(F u) : user(std::move(u)) {}

    template <typename T>
    Result<T> pipe(Result<T> r) {
        if (r.hasValue()) {
            return r;
        }

        return user(r.code());
    }
};

template <typename F>
struct [[nodiscard]] Map : supp::NonCopyable {
    F user;

    explicit Map(F u) : user(std::move(u)) {}

    template <typename T>
    using U = std::invoke_result_t<F, T>;

    template <typename T>
    Result<U<T>> pipe(Result<T> r) {
        if (r.hasValue()) {
            return ok<U<T>>(user(std::move(r).get()));
        }

        return err<U<T>>(r.code());
    }
};

}  // namespace pipe

/*
 * Result<T> -> (T -> Result<U>) -> Result<U>
 */
template <typename F>
auto andThen(F user) {
    return pipe::AndThen{std::move(user)};
}

/*
 * Result<T> -> (ErrCode -> Result<T>) -> Result<T>
 */
template <typename F>
auto orElse(F user) {
    return pipe::OrElse{std::move(user)};
}

/*
 * Result<T> -> (T -> U) -> Result<U>
 */
template <typename F>
auto map(F user) {
    return pipe::Map{std::move(user)};
}

}  // namespace exec
