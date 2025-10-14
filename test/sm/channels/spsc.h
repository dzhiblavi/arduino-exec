#include "Executor.h"

#include <exec/Error.h>
#include <exec/cancel.h>
#include <exec/executor/Executor.h>

#include <utest/utest.h>

namespace test {

using namespace exec;

template <typename SendOp, typename RecvOp, typename Channel>
struct t_spsc_channel {
    struct Task : Runnable {
        Task() = default;

        Runnable* run() override {
            return exec::noop;
        }
    };

    t_spsc_channel() {
        exec::setExecutor(&executor);
    }

    void test_send_nonblocking() {
        TEST_ASSERT_EQUAL(&t1, so1.send(&c, 1, &e1)(&t1));
        TEST_ASSERT_EQUAL(&t1, so2.send(&c, 2, &e2)(&t1));
        TEST_ASSERT_EQUAL(ErrCode::Success, e1);
        TEST_ASSERT_EQUAL(ErrCode::Success, e2);
    }

    void test_receive_nonblocking() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);
        int r1{}, r2{};
        TEST_ASSERT_EQUAL(&t1, ro1.receive(&c, &r1, &e1)(&t1));
        TEST_ASSERT_EQUAL(&t1, ro2.receive(&c, &r2, &e2)(&t1));
        TEST_ASSERT_EQUAL(ErrCode::Success, e1);
        TEST_ASSERT_EQUAL(ErrCode::Success, e2);
        TEST_ASSERT_EQUAL(1, r1);
        TEST_ASSERT_EQUAL(2, r2);
    }

    void test_send_blocking() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);
        TEST_ASSERT_EQUAL(noop, so2.send(&c, 3, &e2)(&t2));
        TEST_ASSERT_EQUAL(ErrCode::Unknown, e2);
    }

    void test_send_blocking_connects_cancellation() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);

        CancellationSignal sig;
        so2.send(&c, 3, &e2)(&t2, sig.slot());
        TEST_ASSERT_TRUE(sig.hasHandler());
    }

    void test_receive_blocking_connects_cancellation() {
        CancellationSignal sig;
        int r{};
        ro1.receive(&c, &r, &e2)(&t2, sig.slot());
        TEST_ASSERT_TRUE(sig.hasHandler());
    }

    void test_send_blocking_cancelled() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);

        CancellationSignal sig;
        so2.send(&c, 3, &e2)(&t2, sig.slot());

        TEST_ASSERT_EQUAL(&t2, sig.emitRaw());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, e2);
        TEST_ASSERT_FALSE(sig.hasHandler());
    }

    void test_receive_blocking_cancelled() {
        CancellationSignal sig;
        int r{};
        ro1.receive(&c, &r, &e2)(&t2, sig.slot());

        TEST_ASSERT_EQUAL(&t2, sig.emitRaw());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, e2);
        TEST_ASSERT_FALSE(sig.hasHandler());
    }

    void test_send_blocking_rendezvouz() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);

        CancellationSignal sig;
        int r{};
        so2.send(&c, 3, &e1)(&t1, sig.slot());

        TEST_ASSERT_EQUAL(&t1, ro1.receive(&c, &r, &e2)(&t2));
        TEST_ASSERT_EQUAL(1, executor.queued.size());
        TEST_ASSERT_EQUAL(&t2, executor.queued.front());

        TEST_ASSERT_EQUAL(ErrCode::Success, e1);
        TEST_ASSERT_EQUAL(1, r);
        TEST_ASSERT_EQUAL(ErrCode::Success, e2);

        TEST_ASSERT_FALSE(sig.hasHandler());
    }

    void test_receive_blocking_rendezvouz() {
        CancellationSignal sig;
        int r{};
        ro1.receive(&c, &r, &e1)(&t1, sig.slot());

        TEST_ASSERT_EQUAL(&t2, so1.send(&c, 3, &e2)(&t2));
        TEST_ASSERT_EQUAL(1, executor.queued.size());
        TEST_ASSERT_EQUAL(&t1, executor.queued.front());

        TEST_ASSERT_EQUAL(ErrCode::Success, e1);
        TEST_ASSERT_EQUAL(3, r);
        TEST_ASSERT_EQUAL(ErrCode::Success, e2);

        TEST_ASSERT_FALSE(sig.hasHandler());
    }

    Channel c;
    SendOp so1, so2, so3;
    RecvOp ro1, ro2, ro3;

    ErrCode e1{ErrCode::Unknown}, e2{ErrCode::Unknown}, e3{ErrCode::Unknown};
    Task t1, t2, t3;
    test::Executor executor;
};

}  // namespace test
