#pragma once

#include "exec/os/OS.h"
#include "exec/os/Service.h"

namespace exec {

template <typename Interface, typename Self>
class ServiceBase : public Service {
 public:
    ServiceBase() {
        if (auto os = service<OS>()) {
            os->addService(this);
        }

        setService<Interface>(static_cast<Self*>(this));
    }
};

}  // namespace exec
