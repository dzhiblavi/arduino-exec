#include "coro/test.h"

#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/wait.h>

#include <utest/utest.h>

void setUp() {
    static_assert(TIME_MANUAL, "manual time is expected for tests");
    ttime::mono::set(ttime::Time());
}

namespace exec {

struct t_wait : t_coro {};

TEST_F(t_wait, waits_the_duration) {
    HeapTimerService<1> service;
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {
        auto errc = co_await wait(ttime::Duration(10)).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(ErrCode::Success, errc);
    }());

    TEST_ASSERT_FALSE(coro.done());  // initial suspend
    coro.start();
    service.tick();
    TEST_ASSERT_EQUAL(1, service.size());
    // should not complete yet (time has not yet come)
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_TRUE(sig.hasHandler());

    // should complete after interval
    ttime::mono::advance(ttime::Duration(10));
    service.tick();
    TEST_ASSERT_FALSE(sig.hasHandler());
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_EQUAL(0, service.size());
}

TEST_F(t_wait, zero_duration) {
    HeapTimerService<1> service;
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {
        auto errc = co_await wait(ttime::Duration(0)).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(ErrCode::Success, errc);
    }());

    TEST_ASSERT_FALSE(coro.done());  // initial suspend
    coro.start();
    TEST_ASSERT_FALSE(sig.hasHandler());
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_EQUAL(0, service.size());
}

TEST_F(t_wait, exhausted) {
    HeapTimerService<0> service;
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {
        auto errc = co_await wait(ttime::Duration(10)).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(ErrCode::Exhausted, errc);
    }());

    TEST_ASSERT_FALSE(coro.done());  // initial suspend
    coro.start();                    // should immediately complete with an error
    TEST_ASSERT_FALSE(sig.hasHandler());
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_wait, cancelled) {
    HeapTimerService<1> service;
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {
        auto errc = co_await wait(ttime::Duration(10)).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, errc);
    }());

    TEST_ASSERT_FALSE(coro.done());  // initial suspend
    coro.start();
    service.tick();
    // should not complete yet (time has not yet come)
    TEST_ASSERT_FALSE(coro.done());

    sig.emit().resume();
    TEST_ASSERT_FALSE(sig.hasHandler());
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_EQUAL(0, service.size());
}

}  // namespace exec

TESTS_MAIN
