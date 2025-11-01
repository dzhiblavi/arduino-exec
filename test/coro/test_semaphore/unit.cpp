#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/sync/Event.h>
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
    auto coro = makeManualTask([&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
    }());

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_semaphore, acquire_unavailable) {
    auto coro = makeManualTask([&]() -> Async<> {  //
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
    }());

    m.tryAcquire();
    m.tryAcquire();

    coro.start();
    TEST_ASSERT_FALSE(coro.done());  // blocked

    m.release();
    TEST_ASSERT_TRUE(coro.done());  // unblocked
}

TEST_F(t_semaphore, acquire_queue) {
    Event e;

    auto make_coro = [&]() -> Async<> {
        LINFO("1");
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
        LINFO("2");
        co_await e.wait();
        LINFO("3");
        m.release();
    };

    auto c1 = makeManualTask(make_coro());
    auto c2 = makeManualTask(make_coro());
    auto c3 = makeManualTask(make_coro());

    c1.start();
    c2.start();
    c3.start();
    TEST_ASSERT_FALSE(c1.done());
    TEST_ASSERT_FALSE(c2.done());
    TEST_ASSERT_FALSE(c3.done());  // blocked

    e.fireOnce();
    TEST_ASSERT_TRUE(c1.done());   // unsuspended
    TEST_ASSERT_TRUE(c2.done());   // unsuspended
    TEST_ASSERT_FALSE(c3.done());  // blocked on e.wait

    e.fireOnce();
    TEST_ASSERT_TRUE(c3.done());  // blocked on e.wait
    TEST_ASSERT_TRUE(m.tryAcquire());
}

TEST_F(t_semaphore, connects_cancellation) {
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {  //
        co_await m.acquire().setCancellationSlot(sig.slot());
    }());

    m.tryAcquire();
    m.tryAcquire();

    coro.start();
    TEST_ASSERT_TRUE(sig.hasHandler());

    m.release();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_semaphore, cancelled) {
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {  //
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, co_await m.acquire().setCancellationSlot(sig.slot()));
    }());

    m.tryAcquire();
    m.tryAcquire();

    coro.start();
    TEST_ASSERT_FALSE(coro.done());  // blocked on lock()
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_TRUE(coro.done());  // released
}

TEST_F(t_semaphore, lock_queue_cancelled) {
    Event e;
    CancellationSignal sig;

    auto make_coro_cancellable = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, co_await m.acquire().setCancellationSlot(sig.slot()));
    };

    auto make_coro = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.acquire());
        co_await e.wait();
        m.release();
    };

    auto c1 = makeManualTask(make_coro());
    auto c2 = makeManualTask(make_coro());
    auto c3 = makeManualTask(make_coro_cancellable());

    c1.start();
    c2.start();
    c3.start();

    TEST_ASSERT_FALSE(c1.done());  // blocked on e.wait
    TEST_ASSERT_FALSE(c2.done());  // blocked on m.acquire
    TEST_ASSERT_FALSE(c3.done());  // blocked on e.wait

    sig.emit();                    // cancel c2
    TEST_ASSERT_FALSE(c1.done());  // blocked on e.wait
    TEST_ASSERT_FALSE(c2.done());  // blocked on e.wait
    TEST_ASSERT_TRUE(c3.done());   // unblocked with no lock

    e.fireOnce();
    TEST_ASSERT_TRUE(c1.done());
    TEST_ASSERT_TRUE(c2.done());
    TEST_ASSERT_TRUE(c3.done());
}

}  // namespace exec

TESTS_MAIN
