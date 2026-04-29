#include "coro/test.h"

#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/defer.h>

#include <utest/utest.h>

void setUp() {
    static_assert(TIME_MANUAL, "manual time is expected for tests");
    ttime::mono::set(ttime::Time());
}

namespace exec {

struct t_defer : t_coro {};

TEST_F(t_defer, waits_the_duration) {
    HeapDeferService<1> service;

    auto coro = makeManualTask([&]() -> Async<> {
        auto errc = co_await defer(ttime::Duration(10));
        TEST_ASSERT_EQUAL(ErrCode::Success, errc);
    }());

    TEST_ASSERT_FALSE(coro.done());  // initial suspend
    coro.start();
    service.tick();
    // should not complete yet (time has not yet come)
    TEST_ASSERT_FALSE(coro.done());

    // should complete after interval
    ttime::mono::advance(ttime::Duration(10));
    service.tick();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_defer, zero_duration) {
    HeapDeferService<1> service;

    auto coro = makeManualTask([&]() -> Async<> {
        auto errc = co_await defer(ttime::Duration(0));
        TEST_ASSERT_EQUAL(ErrCode::Success, errc);
    }());

    TEST_ASSERT_FALSE(coro.done());  // initial suspend
    coro.start();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_defer, exhausted) {
    HeapDeferService<0> service;

    auto coro = makeManualTask([&]() -> Async<> {
        auto errc = co_await defer(ttime::Duration(10));
        TEST_ASSERT_EQUAL(ErrCode::Exhausted, errc);
    }());

    TEST_ASSERT_FALSE(coro.done());  // initial suspend
    coro.start();                    // should immediately complete with an error
    TEST_ASSERT_TRUE(coro.done());
}

}  // namespace exec

TESTS_MAIN
