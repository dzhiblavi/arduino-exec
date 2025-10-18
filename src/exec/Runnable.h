#pragma once

#include <supp/IntrusiveForwardList.h>

#include <type_traits>

namespace exec {

class [[nodiscard]] Runnable : public supp::IntrusiveForwardListNode {
 public:
    virtual ~Runnable() = default;

    // May return a task to run immediadely after
    [[nodiscard]] virtual Runnable* run() = 0;

    void runAll() {
        Runnable* task = run();
        while (task != nullptr) {
            task = task->run();
        }
    }
};

[[maybe_unused]] static constexpr Runnable* noop = nullptr;

namespace detail {

template <typename F>
struct [[nodiscard]] RunnableWrapper : Runnable {
 public:
    RunnableWrapper(F&& closure) : closure_{std::forward<F>(closure)} {}  // NOLINT

    [[nodiscard]] Runnable* run() final {
        if constexpr (std::same_as<decltype(closure_(this)), void>) {
            closure_(this);
            return noop;
        } else {
            return closure_(this);
        }
    }

 private:
    std::remove_reference_t<F> closure_;
};

}  // namespace detail

template <typename F>
using RunnableOf = detail::RunnableWrapper<F>;

template <typename F>
auto runnable(F&& closure) {  // NOLINT
    return detail::RunnableWrapper(std::forward<F>(closure));
}

}  // namespace exec
