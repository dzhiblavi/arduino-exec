#include "sm/channels/spsc.h"

namespace test {

template <typename SendOp, typename RecvOp, typename Channel>
struct t_mpsc_channel : t_spsc_channel<SendOp, RecvOp, Channel> {
    void test_send_blocks_multiple_producers() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);
        TEST_ASSERT_EQUAL(noop, so2.send(&c, 3, &e1)(&t1));
        TEST_ASSERT_EQUAL(noop, so3.send(&c, 4, &e2)(&t2));
    }

    void test_send_blocks_multiple_producers_connects_cancellation() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);

        CancellationSignal s1, s2;
        so2.send(&c, 3, &e1)(&t1, s1.slot());
        so3.send(&c, 4, &e2)(&t2, s2.slot());

        TEST_ASSERT_TRUE(s1.hasHandler());
        TEST_ASSERT_TRUE(s2.hasHandler());
    }

    void test_send_blocks_multiple_producers_both_cancelled() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);

        CancellationSignal s1, s2;
        so2.send(&c, 3, &e1)(&t1, s1.slot());
        so3.send(&c, 4, &e2)(&t2, s2.slot());

        TEST_ASSERT_EQUAL(&t2, s2.emitRaw());
        TEST_ASSERT_FALSE(s2.hasHandler());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, e2);
        TEST_ASSERT_TRUE(s1.hasHandler());

        TEST_ASSERT_EQUAL(&t1, s1.emitRaw());
        TEST_ASSERT_FALSE(s1.hasHandler());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, e1);
    }

    void test_send_blocks_multiple_producers_one_cancelled() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);

        CancellationSignal s1, s2;
        so2.send(&c, 3, &e1)(&t1, s1.slot());
        so3.send(&c, 4, &e2)(&t2, s2.slot());

        (void)s2.emitRaw();

        int r{};
        TEST_ASSERT_EQUAL(&t1, ro1.receive(&c, &r, &e2)(&t2));
        TEST_ASSERT_EQUAL(1, executor.queued.size());
        TEST_ASSERT_EQUAL(&t2, executor.queued.front());
        TEST_ASSERT_EQUAL(ErrCode::Success, e1);
        TEST_ASSERT_EQUAL(ErrCode::Success, e2);
        TEST_ASSERT_EQUAL(1, r);
        TEST_ASSERT_FALSE(s1.hasHandler());
    }

    void test_send_blocks_multiple_producers_both_released() {
        so1.send(&c, 1, &e1)(&t1);
        so1.send(&c, 2, &e1)(&t1);

        CancellationSignal s1, s2;
        so2.send(&c, 3, &e1)(&t1, s1.slot());
        so3.send(&c, 4, &e2)(&t2, s2.slot());

        int r{};
        TEST_ASSERT_EQUAL(&t1, ro1.receive(&c, &r, &e3)(&t3));
        TEST_ASSERT_EQUAL(ErrCode::Success, e3);
        TEST_ASSERT_EQUAL(ErrCode::Success, e1);
        TEST_ASSERT_FALSE(s1.hasHandler());
        TEST_ASSERT_EQUAL(1, executor.queued.size());
        TEST_ASSERT_EQUAL(&t3, executor.queued.front());
        executor.queued.clear();

        TEST_ASSERT_EQUAL(&t2, ro1.receive(&c, &r, &e3)(&t3));
        TEST_ASSERT_EQUAL(ErrCode::Success, e3);
        TEST_ASSERT_EQUAL(ErrCode::Success, e2);
        TEST_ASSERT_FALSE(s2.hasHandler());
        TEST_ASSERT_EQUAL(1, executor.queued.size());
        TEST_ASSERT_EQUAL(&t3, executor.queued.front());
    }

    using tc = t_spsc_channel<SendOp, RecvOp, Channel>;
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
