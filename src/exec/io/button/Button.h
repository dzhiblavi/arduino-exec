#pragma once

#include "exec/sys/config.h"  // IWYU pragma: keep

#ifdef EXEC_ARDUINO

#include "exec/Callback.h"
#include "exec/os/Service.h"

// #define EB_NO_CALLBACK // THIS ENABLES BUGS IN THE LIBRARY
#include <EncButton.h>

namespace exec {

class Button : public Service {
 public:
    using ImplType = ::Button;
    using CallbackArgType = ImplType&;
    using CallbackType = Callback<CallbackArgType>;

    Button();

    bool init(int pin, uint8_t mode, uint8_t level);
    void setCallback(CallbackType* callback);

    ImplType& impl() { return impl_; }

    // Service
    void tick() override;

 private:
    static void EXEC_RAM buttonISR(void* self);

    ImplType impl_;
    CallbackType* callback_ = nullptr;
};

}  // namespace exec

#endif
