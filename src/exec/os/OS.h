#pragma once

#include "exec/os/Service.h"

namespace exec {

class OS;

OS* os();
void setOS(OS* os);

class OS : public Service {
 public:
    OS() {
        setOS(this);
    }

    void tick() override {
        services_.iterate([](Service& s) { s.tick(); });
    }

    void addService(Service* s) {
        services_.pushBack(s);
    }

 private:
    supp::IntrusiveForwardList<Service> services_;
};

}  // namespace exec
