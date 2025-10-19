#pragma once

#include "exec/sys/config.h"  // IWYU pragma: keep

#ifdef EXEC_ARDUINO

// #define EB_NO_CALLBACK // THIS ENABLES BUGS IN THE LIBRARY
#include <EncButton.h>

namespace exec {

class Encoder {
 public:
    using Callback = void (*)(void*);

    Encoder() = default;
    bool init(int s1, int s2, int btn);

    // callback will be executed in tick() call
    void setCallback(Callback callback, void* arg);

    void tick();

    EncButton& impl() {
        return impl_;
    }

 protected:
    EncButton impl_;

 private:
    static void EXEC_RAM encISR(void* self);
    static void EXEC_RAM btnISR(void* self);

    Callback callback_ = nullptr;
    void* arg_ = nullptr;
};

}  // namespace exec

#endif
