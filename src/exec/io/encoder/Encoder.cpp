#include "exec/io/encoder/Encoder.h"

#ifdef EXEC_ARDUINO

#include "exec/io/int/interrupts.h"

namespace exec {

bool Encoder::init(int s1, int s2, int btn) {
    impl_.init(s1, s2, btn, INPUT, INPUT_PULLUP);
    impl_.setEncISR(true);

    attachInterruptArg(digitalPinToInterrupt(s1), Encoder::encISR, this, InterruptMode::Change);
    attachInterruptArg(digitalPinToInterrupt(s2), Encoder::encISR, this, InterruptMode::Change);
    attachInterruptArg(digitalPinToInterrupt(btn), Encoder::btnISR, this, InterruptMode::Falling);

    return true;
}

void Encoder::setCallback(Callback callback, void* arg) {
    callback_ = callback;
    arg_ = arg;
}

void Encoder::tick() {
    impl_.tick();

    if (callback_ != nullptr && impl_.action()) {
        callback_(arg_);
    }
}

void Encoder::encISR(void* self) {
    static_cast<Encoder*>(self)->impl_.tickISR();
}

void Encoder::btnISR(void* self) {
    static_cast<Encoder*>(self)->impl_.pressISR();
}

}  // namespace exec

#endif
