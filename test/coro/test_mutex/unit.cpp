#include "exec/coro/Async.h"

#include <exec/coro/sync/Mutex.h>

#include <utest/utest.h>

namespace exec {

struct t_mutex {
    Mutex m;
};

TEST_F(t_mutex, try_lock) {
    {
        auto guard = m.try_lock();
        TEST_ASSERT_TRUE(guard);
        auto guard2 = m.try_lock();
        TEST_ASSERT_FALSE(guard2);
    }

    TEST_ASSERT_TRUE(m.try_lock());
}

TEST_F(t_mutex, lock_available) {
    auto coro = [&]() -> Async<> {
        auto guard = co_await m.lock();
        TEST_ASSERT_TRUE(guard);
    }();

    TEST_ASSERT_FALSE(coro.done());  // initial suspend
    coro.resume();                   // take lock and release it immediately
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_mutex, lock_reuse_available) {
    auto coro = [&]() -> Async<> {
        {
            auto guard = co_await m.lock();
            TEST_ASSERT_TRUE(guard);
        }

        {
            auto guard = co_await m.lock();
            TEST_ASSERT_TRUE(guard);
        }
    }();

    coro.resume();  // take lock and release it immediately
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_mutex, lock_locked) {
    auto coro = [&]() -> Async<> {
        auto guard = co_await m.lock();
        TEST_ASSERT_TRUE(guard);
    }();

    auto guard = m.try_lock();

    coro.resume();
    TEST_ASSERT_FALSE(coro.done());  // blocked
    std::move(guard).unlock();
    TEST_ASSERT_TRUE(coro.done());  // released and completed
}

TEST_F(t_mutex, lock_queue) {
    auto make_coro = [&]() -> Async<> {
        auto guard = co_await m.lock();
        TEST_ASSERT_TRUE(guard);
        co_await std::suspend_always{};
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

    TEST_ASSERT_TRUE(m.try_lock());
}

TEST_F(t_mutex, connects_cancellation) {
    CancellationSignal sig;
    auto op = m.lock().setCancellationSlot(sig.slot());

    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST_F(t_mutex, cancelled) {
    CancellationSignal sig;

    auto coro = [&]() -> Async<> {
        auto guard = co_await m.lock().setCancellationSlot(sig.slot());
        TEST_ASSERT_FALSE(guard);  // not locked (cancelled)!
    }();

    auto guard = m.try_lock();

    coro.resume();
    TEST_ASSERT_FALSE(coro.done());  // blocked on lock()
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_TRUE(coro.done());  // released
}

TEST_F(t_mutex, lock_queue_cancelled) {
    CancellationSignal sig;

    auto make_coro_cancellable = [&]() -> Async<> {
        auto guard = co_await m.lock().setCancellationSlot(sig.slot());
        TEST_ASSERT_FALSE(guard);
    };

    auto make_coro = [&]() -> Async<> {
        auto guard = co_await m.lock();
        TEST_ASSERT_TRUE(guard);
        co_await std::suspend_always{};
    };

    auto c1 = make_coro();
    auto c2 = make_coro_cancellable();
    auto c3 = make_coro();

    c1.resume();
    c2.resume();
    c3.resume();
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

    TEST_ASSERT_TRUE(m.try_lock());
}

}  // namespace exec

TESTS_MAIN
