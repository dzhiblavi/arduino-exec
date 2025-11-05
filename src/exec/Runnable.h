#pragma once

#include <supp/IntrusiveForwardList.h>

#include <type_traits>

namespace exec {

class [[nodiscard]] Runnable : public supp::IntrusiveForwardListNode<> {
 public:
    virtual ~Runnable() = default;
    virtual void run() = 0;
};

[[maybe_unused]] static constexpr Runnable* noop = nullptr;

namespace detail {

template <typename F>
struct [[nodiscard]] RunnableWrapper : Runnable {
 public:
    RunnableWrapper(F&& closure) : closure_{std::move(closure)} {}
    void run() final { closure_(this); }

 private:
    std::remove_reference_t<F> closure_;
};

}  // namespace detail

template <typename F>
using RunnableOf = detail::RunnableWrapper<F>;

template <typename F>
auto runnable(F&& closure) {
    return detail::RunnableWrapper(std::forward<F>(closure));
}

}  // namespace exec
