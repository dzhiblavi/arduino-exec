#pragma once

#include "exec/os/Service.h"

namespace exec {

class OS : public Service {
 public:
    template <typename... S>
    explicit OS(S*... services) {
        (addService(services), ...);
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
