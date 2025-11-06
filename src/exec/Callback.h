#pragma once

#include <type_traits>
#include <utility>

namespace exec {

template <typename T>
class Callback {
 public:
    virtual ~Callback() = default;
    virtual void run(T) = 0;
};

namespace detail {

template <typename T, typename F>
class CallbackWrapper : public Callback<T> {
 public:
    template <typename A>
    CallbackWrapper(A&& f) : func_{std::forward<A>(f)} {}
    void run(T value) override { func_(std::forward<T>(value)); }

 private:
    std::remove_reference_t<F> func_;
};

}  // namespace detail

template <typename T, typename F>
auto makeCallback(F&& func) {
    return detail::CallbackWrapper<T, F>{std::forward<F>(func)};
}

}  // namespace exec
