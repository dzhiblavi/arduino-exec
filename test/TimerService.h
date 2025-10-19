#pragma once

#include "exec/os//TimerService.h"
#include <unity.h>

namespace test {

class TimerService : public exec::TimerService {
 public:
    ~TimerService() {
        TEST_ASSERT_EQUAL(nullptr, next_add);
        TEST_ASSERT_EQUAL(nullptr, next_remove);
    }

    void add(exec::TimerEntry* t) override {
        TEST_ASSERT_EQUAL(next_add, t->task);
        next_add = nullptr;
        last_added = t->task;
    }

    bool remove(exec::TimerEntry* t) override {
        TEST_ASSERT_EQUAL(next_remove, t->task);
        next_remove = nullptr;
        return remove_result;
    }

    void* next_add = nullptr;
    exec::Runnable* last_added = nullptr;
    void* next_remove = nullptr;
    bool remove_result = false;
};

}  // namespace test
