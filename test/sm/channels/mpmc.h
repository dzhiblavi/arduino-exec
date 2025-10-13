#include "sm/channels/mpsc.h"

namespace test {

template <typename SendOp, typename RecvOp, typename Channel>
struct t_mpmc_channel : t_mpsc_channel<SendOp, RecvOp, Channel> {
    void test_receive_blocks_multiple_consumers() {
        int r1{}, r2{};
        TEST_ASSERT_EQUAL(noop, ro1.receive(&c, &r1, &e1)(&t1));
        TEST_ASSERT_EQUAL(noop, ro2.receive(&c, &r2, &e2)(&t2));
    }

    void test_receive_blocks_multiple_consumers_connects_cancellation() {
        int r1{}, r2{};
        CancellationSignal s1, s2;
        ro1.receive(&c, &r1, &e1)(&t1, s1.slot());
        ro2.receive(&c, &r2, &e2)(&t2, s2.slot());

        TEST_ASSERT_TRUE(s1.hasHandler());
        TEST_ASSERT_TRUE(s2.hasHandler());
    }

    void test_receive_blocks_multiple_consumers_both_cancelled() {
        int r1{}, r2{};
        CancellationSignal s1, s2;
        ro1.receive(&c, &r1, &e1)(&t1, s1.slot());
        ro2.receive(&c, &r2, &e2)(&t2, s2.slot());

        TEST_ASSERT_EQUAL(&t2, s2.emitRaw());
        TEST_ASSERT_FALSE(s2.hasHandler());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, e2);
        TEST_ASSERT_TRUE(s1.hasHandler());

        TEST_ASSERT_EQUAL(&t1, s1.emitRaw());
        TEST_ASSERT_FALSE(s1.hasHandler());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, e1);
    }

    void test_receive_blocks_multiple_consumers_one_cancelled() {
        int r1{}, r2{};
        CancellationSignal s1, s2;
        ro1.receive(&c, &r1, &e1)(&t1, s1.slot());
        ro2.receive(&c, &r2, &e2)(&t2, s2.slot());

        (void)s2.emitRaw();

        TEST_ASSERT_EQUAL(&t3, so1.send(&c, 1, &e3)(&t3));
        TEST_ASSERT_EQUAL(1, executor.queued.size());
        TEST_ASSERT_EQUAL(&t1, executor.queued.front());
        TEST_ASSERT_EQUAL(ErrCode::Success, e1);
        TEST_ASSERT_EQUAL(ErrCode::Success, e3);
        TEST_ASSERT_EQUAL(1, r1);
        TEST_ASSERT_FALSE(s1.hasHandler());
    }

    void test_receive_blocks_multiple_consumers_both_released() {
        int r1{}, r2{};
        CancellationSignal s1, s2;
        ro1.receive(&c, &r1, &e1)(&t1, s1.slot());
        ro2.receive(&c, &r2, &e2)(&t2, s2.slot());

        TEST_ASSERT_EQUAL(&t3, so1.send(&c, 1, &e3)(&t3));
        TEST_ASSERT_EQUAL(1, executor.queued.size());
        TEST_ASSERT_EQUAL(&t1, executor.queued.front());
        TEST_ASSERT_EQUAL(ErrCode::Success, e1);
        TEST_ASSERT_EQUAL(ErrCode::Success, e3);
        TEST_ASSERT_FALSE(s1.hasHandler());
        TEST_ASSERT_EQUAL(r1, 1);
        executor.queued.clear();

        TEST_ASSERT_EQUAL(&t3, so1.send(&c, 2, &e3)(&t3));
        TEST_ASSERT_EQUAL(1, executor.queued.size());
        TEST_ASSERT_EQUAL(&t2, executor.queued.front());
        TEST_ASSERT_EQUAL(ErrCode::Success, e2);
        TEST_ASSERT_EQUAL(ErrCode::Success, e3);
        TEST_ASSERT_FALSE(s2.hasHandler());
        TEST_ASSERT_EQUAL(r2, 2);
    }

    using tc = t_mpsc_channel<SendOp, RecvOp, Channel>;
    using tc::c;
    using tc::e1;
    using tc::e2;
    using tc::e3;
    using tc::executor;
    using tc::ro1;
    using tc::ro2;
    using tc::ro3;
    using tc::so1;
    using tc::so2;
    using tc::so3;
    using tc::t1;
    using tc::t2;
    using tc::t3;
};

}  // namespace test
