#include "Executor.h"

#include <exec/sm/detach.h>

#include <utest/utest.h>

namespace exec {

struct t_detach {
    struct TaskState {
        CancellationSlot slot;
        bool cancel_called = false;

        Runnable* start_result = noop;
        Runnable* saved_init_arg = noop;
    };

    struct Task : CancellationHandler {
        Task(TaskState* state) : state{state} {}

        Initiator auto start() {
            return [this](Runnable* cb, CancellationSlot slot = {}) {  //
                state->slot = slot;
                state->slot.installIfConnected(this);
                state->saved_init_arg = cb;
                return state->start_result;
            };
        }

        Runnable* finish() {
            return noop;
        }

        Runnable* cancel() override {
            state->slot.clearIfConnected();
            state->cancel_called = true;
            return noop;
        }

        TaskState* state;
    };

    t_detach() {
        setExecutor(&executor);
    }

    TaskState s;
    Task t{&s};
    test::Executor executor;
};

TEST_F(t_detach, test_detach_completed) {
    auto t1 = task([](auto*) { return noop; });
    auto t2 = task([](auto*) { return noop; });

    auto initiator = detach(t);
    TEST_ASSERT_EQUAL(noop, s.saved_init_arg);

    s.start_result = &t2;
    TEST_ASSERT_EQUAL(s.start_result, initiator(&t1));

    TEST_ASSERT_EQUAL(&t1, s.saved_init_arg->run());
}

TEST_F(t_detach, test_detach_cancelled) {
    auto t1 = task([](auto*) { return noop; });
    auto t2 = task([](auto*) { return noop; });

    CancellationSignal sig;
    auto initiator = detach(t);

    s.start_result = &t2;
    initiator(&t1, sig.slot());

    sig.emit();
    TEST_ASSERT_TRUE(s.cancel_called);
    s.saved_init_arg->runAll();
}

TEST_F(t_detach, test_spawn) {
    auto t1 = task([](auto*) { return noop; });

    s.start_result = &t1;
    spawn(t);
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(s.start_result, executor.queued.front());
    TEST_ASSERT_EQUAL(noop, s.saved_init_arg->run());
}

}  // namespace exec

TESTS_MAIN
