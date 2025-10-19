#pragma once

#include "exec/os/Service.h"

namespace exec {

class OS : public Service {
 public:
    OS();
    void tick() override;
    void addService(Service* s);

 private:
    supp::IntrusiveForwardList<Service> services_;
};

OS* os();
void setOS(OS* os);

}  // namespace exec
