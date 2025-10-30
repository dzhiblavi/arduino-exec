#include "exec/coro/Async.h"

#include <exec/coro/sync/Semaphore.h>

#include <utest/utest.h>

namespace exec {

struct t_semaphore {
    Semaphore m{2};
};

TEST_F(t_semaphore, try_acquire) {
    TEST_ASSERT_TRUE(m.tryAcquire());
    TEST_ASSERT_TRUE(m.tryAcquire());
    TEST_ASSERT_FALSE(m.tryAcquire());

    m.release();
    TEST_ASSERT_TRUE(m.tryAcquire());
    TEST_ASSERT_FALSE(m.tryAcquire());
}

TEST_F(t_semaphore, acquire_available) {
    auto coro = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
    }();

    coro.resume();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_semaphore, acquire_unavailable) {
    auto coro = [&]() -> Async<> {  //
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
    }();

    m.tryAcquire();
    m.tryAcquire();

    coro.resume();
    TEST_ASSERT_FALSE(coro.done());  // blocked

    m.release();
    TEST_ASSERT_TRUE(coro.done());  // unblocked
}

TEST_F(t_semaphore, acquire_queue) {
    auto make_coro = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
        co_await std::suspend_always{};
        m.release();
    };

    auto c1 = make_coro();
    auto c2 = make_coro();
    auto c3 = make_coro();

    c1.resume();
    c2.resume();
    c3.resume();
    TEST_ASSERT_FALSE(c1.done());
    TEST_ASSERT_FALSE(c2.done());  // blocked
    TEST_ASSERT_FALSE(c3.done());  // blocked

    c1.resume();
    TEST_ASSERT_TRUE(c1.done());   // unsuspended
    TEST_ASSERT_FALSE(c2.done());  // unblocked, suspended
    TEST_ASSERT_FALSE(c3.done());  // blocked

    c2.resume();
    TEST_ASSERT_TRUE(c2.done());   // unsuspended
    TEST_ASSERT_FALSE(c3.done());  // unblocked, suspended

    c3.resume();
    TEST_ASSERT_TRUE(c3.done());  // unsuspended

    TEST_ASSERT_TRUE(m.tryAcquire());
}

TEST_F(t_semaphore, connects_cancellation) {
    CancellationSignal sig;
    auto op = m.acquire().setCancellationSlot(sig.slot());

    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST_F(t_semaphore, cancelled) {
    CancellationSignal sig;

    auto coro = [&]() -> Async<> {  //
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, co_await m.acquire().setCancellationSlot(sig.slot()));
    }();

    m.tryAcquire();
    m.tryAcquire();

    coro.resume();
    TEST_ASSERT_FALSE(coro.done());  // blocked on lock()
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_TRUE(coro.done());  // released
}

TEST_F(t_semaphore, lock_queue_cancelled) {
    CancellationSignal sig;

    auto make_coro_cancellable = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, co_await m.acquire().setCancellationSlot(sig.slot()));
    };

    auto make_coro = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
        co_await std::suspend_always{};
        m.release();
    };

    auto c1 = make_coro();
    auto c2 = make_coro_cancellable();
    auto c3 = make_coro();

    c1.resume();
    c3.resume();
    c2.resume();
    TEST_ASSERT_FALSE(c1.done());
    TEST_ASSERT_FALSE(c2.done());  // blocked
    TEST_ASSERT_FALSE(c3.done());  // blocked

    sig.emit();  // cancel c2
    TEST_ASSERT_FALSE(c1.done());
    TEST_ASSERT_TRUE(c2.done());   // unblocked with no lock
    TEST_ASSERT_FALSE(c3.done());  // blocked

    c1.resume();
    TEST_ASSERT_TRUE(c1.done());
    TEST_ASSERT_FALSE(c3.done());  // suspended

    c3.resume();
    TEST_ASSERT_TRUE(c3.done());  // unsuspended

    TEST_ASSERT_TRUE(m.tryAcquire());
}

}  // namespace exec

TESTS_MAIN
