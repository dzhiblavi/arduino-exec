#pragma once

#include <time/time.h>
#include <supp/IntrusiveForwardList.h>

namespace exec {

class Service : public supp::IntrusiveForwardListNode<> {
 public:
    virtual ~Service() = default;
    virtual void tick() = 0;
    virtual ttime::Time wakeAt() const; // ttime::Time::max() by default
};

}  // namespace exec
