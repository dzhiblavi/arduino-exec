#pragma once

#include <supp/IntrusiveForwardList.h>
#include <time/time.h>

namespace exec {

class Service : public supp::IntrusiveForwardListNode<> {
 public:
    virtual ~Service() = default;
    virtual void tick() = 0;
    virtual ttime::Time wakeAt() const;  // ttime::Time::max() by default
};

namespace detail {

template <typename I>
inline I* instance_ = nullptr;

}  // namespace detail

template <typename I>
I* tryService() {
    return detail::instance_<I>;
}

template <typename I>
I* service() {
    return VERIFY(tryService<I>(), F("Service not registered"));
}

template <typename I, typename S>
void setService(S* instance) {
    auto* next = static_cast<I*>(instance);
    auto*& current = detail::instance_<I>;

    DASSERT(next != nullptr, F("Cannot register null service"));
    DASSERT(current == nullptr, F("Service already registered"));

    if (current == nullptr) {
        current = next;
    }
}

template <typename I, typename S>
void unsetService(S* instance) {
    auto* expected = static_cast<I*>(instance);
    auto*& current = detail::instance_<I>;

    DASSERT(expected != nullptr, F("Cannot unregister null service"));
    DASSERT(current == expected, F("Another service is registered"));

    if (current == expected) {
        current = nullptr;
    }
}

}  // namespace exec
