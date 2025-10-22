#pragma once

#include "exec/os/Service.h"

namespace exec {

class OS : public Service {
 public:
    OS();
    void addService(Service* s);

    // Service
    void tick() override;
    ttime::Time wakeAt() const override;

 private:
    supp::IntrusiveForwardList<Service> services_;
};

}  // namespace exec
