#pragma once

#include <supp/IntrusiveForwardList.h>

namespace exec {

class Service : public supp::IntrusiveForwardListNode {
 public:
    virtual ~Service() = default;
    virtual void tick() = 0;
};

}  // namespace exec
