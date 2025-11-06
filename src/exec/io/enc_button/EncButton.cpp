#include "exec/io/enc_button/EncButton.h"

#ifdef EXEC_ARDUINO

#include "exec/io/int/interrupts.h"
#include "exec/os/OS.h"

namespace exec {

EncButton::EncButton() {
    OS::globalAddService(this);
}

bool EncButton::init(int s1, int s2, int btn) {
    impl_.init(s1, s2, btn, INPUT, INPUT_PULLUP);
    impl_.setEncISR(true);

    attachInterruptArg(digitalPinToInterrupt(s1), EncButton::encISR, this, InterruptMode::Change);
    attachInterruptArg(digitalPinToInterrupt(s2), EncButton::encISR, this, InterruptMode::Change);

    if (int i = digitalPinToInterrupt(btn); i != -1) {
        attachInterruptArg(i, EncButton::btnISR, this, InterruptMode::Falling);
    }

    return true;
}

void EncButton::setCallback(CallbackType* callback) {
    callback_ = callback;
}

void EncButton::tick() {
    impl_.tick();

    if (callback_ != nullptr && impl_.action()) {
        callback_->run(impl_);
    }
}

void EncButton::encISR(void* self) {
    static_cast<EncButton*>(self)->impl_.tickISR();
}

void EncButton::btnISR(void* self) {
    static_cast<EncButton*>(self)->impl_.pressISR();
}

}  // namespace exec

#endif
