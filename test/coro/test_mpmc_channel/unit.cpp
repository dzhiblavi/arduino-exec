#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/sync/MPMCChannel.h>

#include <utest/utest.h>

namespace exec {

struct t_mpmc_channel {
    auto receiver(int x) {
        return makeManualTask([this](int x) -> Async<> {
            auto y = co_await c.receive();

            if (x == -1) {
                TEST_ASSERT_FALSE(y.has_value());
            } else {
                TEST_ASSERT_TRUE(y.has_value());
                TEST_ASSERT_EQUAL(x, *y);
            }
        }(x));
    }

    auto sender(int x, ErrCode expected) {
        return makeManualTask([this](int x, ErrCode expected) mutable -> Async<> {
            auto ec = co_await c.send(x);
            TEST_ASSERT_EQUAL(expected, ec);
        }(x, expected));
    }

    MPMCChannel<int, 2> c;
};

TEST_F(t_mpmc_channel, receive_blocks_on_empty) {
    auto r = receiver(10);
    auto s = sender(10, ErrCode::Success);

    r.start();
    TEST_ASSERT_FALSE(r.done());

    s.start();
    TEST_ASSERT_TRUE(s.done());
    TEST_ASSERT_TRUE(r.done());
}

TEST_F(t_mpmc_channel, sender_does_not_block_until_filled) {
    auto s1 = sender(10, ErrCode::Success);
    auto s2 = sender(20, ErrCode::Success);

    s1.start();
    TEST_ASSERT_TRUE(s1.done());

    s2.start();
    TEST_ASSERT_TRUE(s2.done());
}

TEST_F(t_mpmc_channel, sender_blocked_when_filled) {
    auto s1 = sender(10, ErrCode::Success);
    auto s2 = sender(20, ErrCode::Success);
    auto s3 = sender(30, ErrCode::Success);

    s1.start();
    TEST_ASSERT_TRUE(s1.done());

    s2.start();
    TEST_ASSERT_TRUE(s2.done());

    s3.start();
    TEST_ASSERT_FALSE(s3.done());

    auto r = receiver(10);  // 10 -- check FIFO
    r.start();
    TEST_ASSERT_TRUE(r.done());
    TEST_ASSERT_TRUE(s3.done());
}

TEST_F(t_mpmc_channel, fifo_no_blocking) {
    auto s1 = sender(10, ErrCode::Success);
    auto s2 = sender(20, ErrCode::Success);

    s1.start();
    TEST_ASSERT_TRUE(s1.done());

    s2.start();
    TEST_ASSERT_TRUE(s2.done());

    auto r1 = receiver(10);
    auto r2 = receiver(20);

    r1.start();
    TEST_ASSERT_TRUE(r1.done());

    r2.start();
    TEST_ASSERT_TRUE(r2.done());
}

TEST_F(t_mpmc_channel, fifo_blocked_senders) {
    auto s1 = sender(10, ErrCode::Success);
    auto s2 = sender(20, ErrCode::Success);
    auto s3 = sender(30, ErrCode::Success);
    auto s4 = sender(40, ErrCode::Success);

    s1.start();
    TEST_ASSERT_TRUE(s1.done());
    s2.start();
    TEST_ASSERT_TRUE(s2.done());
    s3.start();
    TEST_ASSERT_FALSE(s3.done());
    s4.start();
    TEST_ASSERT_FALSE(s4.done());

    auto r1 = receiver(10);
    auto r2 = receiver(20);
    auto r3 = receiver(30);
    auto r4 = receiver(40);

    r1.start();
    TEST_ASSERT_TRUE(r1.done());
    r2.start();
    TEST_ASSERT_TRUE(r2.done());
    r3.start();
    TEST_ASSERT_TRUE(r3.done());
    r4.start();
    TEST_ASSERT_TRUE(r4.done());
}

TEST_F(t_mpmc_channel, fifo_blocked_receivers) {
    auto r1 = receiver(10);
    auto r2 = receiver(20);
    auto r3 = receiver(30);
    auto r4 = receiver(40);

    r1.start();
    TEST_ASSERT_FALSE(r1.done());
    r2.start();
    TEST_ASSERT_FALSE(r2.done());
    r3.start();
    TEST_ASSERT_FALSE(r3.done());
    r4.start();
    TEST_ASSERT_FALSE(r4.done());

    auto s1 = sender(10, ErrCode::Success);
    auto s2 = sender(20, ErrCode::Success);
    auto s3 = sender(30, ErrCode::Success);
    auto s4 = sender(40, ErrCode::Success);

    s1.start();
    TEST_ASSERT_TRUE(s1.done());
    s2.start();
    TEST_ASSERT_TRUE(s2.done());
    s3.start();
    TEST_ASSERT_TRUE(s3.done());
    s4.start();
    TEST_ASSERT_TRUE(s4.done());

    TEST_ASSERT_TRUE(r1.done());
    TEST_ASSERT_TRUE(r2.done());
    TEST_ASSERT_TRUE(r3.done());
    TEST_ASSERT_TRUE(r4.done());
}

TEST_F(t_mpmc_channel, receive_cancellation) {
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {
        auto res = co_await c.receive().setCancellationSlot(sig.slot());
        TEST_ASSERT_FALSE(res.has_value());
    }());

    TEST_ASSERT_FALSE(sig.hasHandler());
    coro.start();
    TEST_ASSERT_TRUE(sig.hasHandler());
    TEST_ASSERT_FALSE(coro.done());

    sig.emit();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_mpmc_channel, send_cancellation) {
    CancellationSignal sig;

    auto s1 = sender(10, ErrCode::Success);
    auto s2 = sender(20, ErrCode::Success);
    auto coro = makeManualTask([&]() -> Async<> {
        int x = 30;
        auto ec = co_await c.send(x).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);
    }());

    s1.start();
    s2.start();

    TEST_ASSERT_FALSE(sig.hasHandler());
    coro.start();
    TEST_ASSERT_TRUE(sig.hasHandler());
    TEST_ASSERT_FALSE(coro.done());

    sig.emit();
    TEST_ASSERT_TRUE(coro.done());
}

}  // namespace exec

TESTS_MAIN
