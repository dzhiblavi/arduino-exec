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
    detail::instance_<I> = instance;
}

}  // namespace exec
