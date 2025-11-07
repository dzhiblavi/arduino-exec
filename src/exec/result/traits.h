#pragma once

#include "exec/result/Result.h"

namespace exec::traits {

namespace detail {

template <typename T>
struct IsResult : std::false_type {};

template <typename T>
struct IsResult<Result<T>> : std::true_type {};

}  // namespace detail

template <typename T>
concept IsResult = detail::IsResult<T>::value;

template <IsResult R>
using ValueOf = typename R::ValueType;

}  // namespace exec::traits
