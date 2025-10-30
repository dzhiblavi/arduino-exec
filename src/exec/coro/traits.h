#pragma once

#include <utility>

namespace exec {

template <class, class = std::void_t<>>
struct has_co_await_operator {
    static constexpr bool value = false;
};

template <class T>
struct has_co_await_operator<T, std::void_t<decltype(std::declval<T>().operator co_await())>> {
    static constexpr bool value = true;
};

template <class T>
constexpr bool has_co_await_operator_v = has_co_await_operator<T>::value;

template <class T, bool HasCoAwait>
struct get_awaiter {
    using type = decltype(std::declval<T>().operator co_await());
};

template <class T>
struct get_awaiter<T, false> {
    using type = T;
};

template <class T>
using get_awaiter_t = typename get_awaiter<T, has_co_await_operator_v<T>>::type;

template <class Awaitable>
struct awaitable_result {
    using type = decltype(std::declval<get_awaiter_t<Awaitable>>().await_resume());
};

template <class T>
using awaitable_result_t = typename awaitable_result<T>::type;

}  // namespace exec
