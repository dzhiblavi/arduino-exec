#include <exec/Error.h>
#include <exec/sm/par/Any.h>

#include <utest/utest.h>

namespace exec {

struct TaskConf {
    Runnable* init_result = noop;
    Runnable* cancel_result = noop;
};

struct Task : CancellationHandler {
    Task(TaskConf c) : c(c) {}

    Initiator auto operator()() {
        return [this](Runnable* r, CancellationSlot slot = {}) {
            this->init_runnable = r;
            this->slot = slot;
            this->slot.installIfConnected(this);
            return c.init_result;
        };
    }

    Runnable* cancel() override {
        cancel_called = true;
        slot.clearIfConnected();
        return c.cancel_result;
    }

    TaskConf c;
    CancellationSlot slot;
    Runnable* init_runnable = nullptr;
    bool cancel_called = false;
};

struct Callback : Runnable {
    Runnable* run() override {
        called = true;
        return noop;
    }

    bool called = false;
};

struct t_any {
    Any<2> a;
    Callback cb;
};

TEST_F(t_any, test_any_no_child_continuations) {
    Task t1({});
    Task t2({});

    TEST_ASSERT_EQUAL(noop, a(t1(), t2())(&cb));
}

TEST_F(t_any, test_any_with_child_continuations) {
    Callback cb;
    Task t1({.init_result = &cb});
    Task t2({});

    auto r = a(t1(), t2())(&cb);
    TEST_ASSERT_EQUAL(&a, r);
    TEST_ASSERT_FALSE(cb.called);
    TEST_ASSERT_EQUAL(noop, r->run());
    TEST_ASSERT_TRUE(cb.called);
}

TEST_F(t_any, test_any_installs_cancellations) {
    CancellationSignal s;
    Callback cb;
    Task t1({});
    Task t2({});

    a(t1(), t2())(&cb, s.slot());
    TEST_ASSERT_TRUE(s.hasHandler());
    TEST_ASSERT_TRUE(t1.slot.hasHandler());
    TEST_ASSERT_TRUE(t2.slot.hasHandler());
}

TEST_F(t_any, test_any_both_complete) {
    CancellationSignal s;
    Callback cb, cb_cancel;
    Task t1({});
    Task t2({.cancel_result = &cb_cancel});

    a(t1(), t2())(&cb, s.slot());
    auto r = t1.init_runnable->run();
    TEST_ASSERT_EQUAL(&a, r);
    TEST_ASSERT_TRUE(!t1.cancel_called & t2.cancel_called);
    TEST_ASSERT_FALSE(cb_cancel.called);
    TEST_ASSERT_EQUAL(noop, r->run());
    TEST_ASSERT_TRUE(cb_cancel.called);

    TEST_ASSERT_FALSE(cb.called);
    TEST_ASSERT_EQUAL(&cb, t2.init_runnable->run());

    TEST_ASSERT_TRUE(!t1.cancel_called & t2.cancel_called);
    TEST_ASSERT_FALSE(s.hasHandler());
}

TEST_F(t_any, test_any_neither_complete) {
    CancellationSignal s;
    Callback cb;
    Task t1({});
    Task t2({});

    a(t1(), t2())(&cb, s.slot());
    TEST_ASSERT_EQUAL(noop, s.emitRaw());
    TEST_ASSERT_TRUE(t1.cancel_called && t2.cancel_called);
    TEST_ASSERT_FALSE(s.hasHandler());
}

}  // namespace exec

TESTS_MAIN
