#include "exec/io/button/Button.h"

#ifdef EXEC_ARDUINO

#include "exec/io/int/interrupts.h"
#include "exec/os/OS.h"

namespace exec {

Button::Button() {
    OS::globalAddService(this);
}

bool Button::init(int pin, uint8_t mode, uint8_t level) {
    impl_.init(pin, mode, level);
    attachInterruptArg(digitalPinToInterrupt(pin), Button::buttonISR, this, InterruptMode::Change);
    return true;
}

void Button::setCallback(CallbackType* callback) {
    callback_ = callback;
}

void Button::tick() {
    impl_.tick();

    if (callback_ != nullptr && impl_.action()) {
        callback_->run(impl_);
    }
}

void Button::buttonISR(void* self) {
    static_cast<Button*>(self)->impl_.pressISR();
}

}  // namespace exec

#endif
