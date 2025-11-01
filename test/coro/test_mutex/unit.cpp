#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/sync/Event.h>
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
    auto coro = makeManualTask([&]() -> Async<> {
        auto guard = co_await m.lock();
        TEST_ASSERT_TRUE(guard);
    }());

    TEST_ASSERT_FALSE(coro.done());  // initial suspend
    coro.start();                    // take lock and release it immediately
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_mutex, lock_reuse_available) {
    auto coro = makeManualTask([&]() -> Async<> {
        {
            auto guard = co_await m.lock();
            TEST_ASSERT_TRUE(guard);
        }

        {
            auto guard = co_await m.lock();
            TEST_ASSERT_TRUE(guard);
        }
    }());

    coro.start();  // take lock and release it immediately
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_mutex, lock_locked) {
    auto coro = makeManualTask([&]() -> Async<> {
        auto guard = co_await m.lock();
        TEST_ASSERT_TRUE(guard);
    }());

    auto guard = m.try_lock();

    coro.start();
    TEST_ASSERT_FALSE(coro.done());  // blocked
    std::move(guard).unlock();
    TEST_ASSERT_TRUE(coro.done());  // released and completed
}

TEST_F(t_mutex, lock_queue) {
    Event e;

    auto make_coro = [&]() -> Async<> {
        auto guard = co_await m.lock();
        TEST_ASSERT_TRUE(guard);
        co_await e.wait();
    };

    auto c1 = makeManualTask(make_coro());
    auto c2 = makeManualTask(make_coro());
    auto c3 = makeManualTask(make_coro());

    c1.start();
    c2.start();
    c3.start();
    TEST_ASSERT_FALSE(c1.done());
    TEST_ASSERT_FALSE(c2.done());  // blocked
    TEST_ASSERT_FALSE(c3.done());  // blocked

    e.fireOnce();
    TEST_ASSERT_TRUE(c1.done());   // unsuspended
    TEST_ASSERT_FALSE(c2.done());  // unblocked, suspended
    TEST_ASSERT_FALSE(c3.done());  // blocked

    e.fireOnce();
    TEST_ASSERT_TRUE(c2.done());   // unsuspended
    TEST_ASSERT_FALSE(c3.done());  // unblocked, suspended

    e.fireOnce();
    TEST_ASSERT_TRUE(c3.done());  // unsuspended

    TEST_ASSERT_TRUE(m.try_lock());
}

TEST_F(t_mutex, connects_cancellation) {
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {
        auto guard = co_await m.lock().setCancellationSlot(sig.slot());
    }());

    auto guard = m.try_lock();

    coro.start();
    TEST_ASSERT_TRUE(sig.hasHandler());

    std::move(guard).unlock();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_mutex, cancelled) {
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {
        auto guard = co_await m.lock().setCancellationSlot(sig.slot());
        TEST_ASSERT_FALSE(guard);  // not locked (cancelled)!
    }());

    auto guard = m.try_lock();

    coro.start();
    TEST_ASSERT_FALSE(coro.done());  // blocked on lock()
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_TRUE(coro.done());  // released
}

TEST_F(t_mutex, lock_queue_cancelled) {
    Event e;
    CancellationSignal sig;

    auto make_coro_cancellable = [&]() -> Async<> {
        auto guard = co_await m.lock().setCancellationSlot(sig.slot());
        TEST_ASSERT_FALSE(guard);
    };

    auto make_coro = [&]() -> Async<> {
        auto guard = co_await m.lock();
        TEST_ASSERT_TRUE(guard);
        co_await e.wait();
    };

    auto c1 = makeManualTask(make_coro());
    auto c2 = makeManualTask(make_coro_cancellable());
    auto c3 = makeManualTask(make_coro());

    c1.start();
    c2.start();
    c3.start();
    TEST_ASSERT_FALSE(c1.done());
    TEST_ASSERT_FALSE(c2.done());  // blocked
    TEST_ASSERT_FALSE(c3.done());  // blocked

    sig.emit();  // cancel c2
    TEST_ASSERT_FALSE(c1.done());
    TEST_ASSERT_TRUE(c2.done());   // unblocked with no lock
    TEST_ASSERT_FALSE(c3.done());  // blocked

    e.fireOnce();
    TEST_ASSERT_TRUE(c1.done());
    TEST_ASSERT_FALSE(c3.done());  // suspended

    e.fireOnce();
    TEST_ASSERT_TRUE(c3.done());  // unsuspended

    TEST_ASSERT_TRUE(m.try_lock());
}

}  // namespace exec

TESTS_MAIN
