#include "exec/coro/Async.h"

#include <exec/coro/sync/Event.h>

#include <utest/utest.h>

namespace exec {

struct t_event {
    Event m;
};

TEST_F(t_event, isSet) {
    TEST_ASSERT_FALSE(m.isSet());
    m.fireOnce();
    TEST_ASSERT_FALSE(m.isSet());
    m.set();
    TEST_ASSERT_TRUE(m.isSet());
    TEST_ASSERT_TRUE(m.isSet());
}

TEST_F(t_event, fire_once) {
    auto coro = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
    }();

    coro.resume();
    TEST_ASSERT_FALSE(coro.done());  // blocked
    m.fireOnce();
    TEST_ASSERT_FALSE(coro.done());  // blocked
    m.fireOnce();
    TEST_ASSERT_TRUE(coro.done());  // released
}

TEST_F(t_event, set) {
    auto coro = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
    }();

    coro.resume();
    TEST_ASSERT_FALSE(coro.done());  // blocked
    m.set();
    TEST_ASSERT_TRUE(coro.done());  // unblocked all
}

TEST_F(t_event, clear) {
    auto coro = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
        m.clear();
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
    }();

    coro.resume();
    TEST_ASSERT_FALSE(coro.done());  // blocked
    m.fireOnce();
    TEST_ASSERT_FALSE(coro.done());  // blocked
    m.fireOnce();
    TEST_ASSERT_TRUE(coro.done());  // released
}

TEST_F(t_event, wait_queue_set) {
    auto make_coro = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
    };

    auto c1 = make_coro();
    auto c2 = make_coro();
    auto c3 = make_coro();

    c1.resume();
    c2.resume();
    c3.resume();
    TEST_ASSERT_FALSE(c1.done());  // blocked
    TEST_ASSERT_FALSE(c2.done());  // blocked
    TEST_ASSERT_FALSE(c3.done());  // blocked

    m.set();
    TEST_ASSERT_TRUE(c1.done());
    TEST_ASSERT_TRUE(c2.done());
    TEST_ASSERT_TRUE(c3.done());
}

TEST_F(t_event, wait_queue_fire_once) {
    auto make_coro = [&]() -> Async<> {
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
    };

    auto c1 = make_coro();
    auto c2 = make_coro();
    auto c3 = make_coro();

    c1.resume();
    c2.resume();
    c3.resume();
    TEST_ASSERT_FALSE(c1.done());  // blocked
    TEST_ASSERT_FALSE(c2.done());  // blocked
    TEST_ASSERT_FALSE(c3.done());  // blocked

    m.fireOnce();
    TEST_ASSERT_FALSE(c1.done());
    TEST_ASSERT_FALSE(c2.done());
    TEST_ASSERT_FALSE(c3.done());

    m.fireOnce();
    TEST_ASSERT_TRUE(c1.done());
    TEST_ASSERT_TRUE(c2.done());
    TEST_ASSERT_TRUE(c3.done());
}

TEST_F(t_event, connects_cancellation) {
    CancellationSignal sig;
    [[maybe_unused]] auto&& op = m.wait().setCancellationSlot(sig.slot());

    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST_F(t_event, cancelled) {
    CancellationSignal sig;

    auto coro = [&]() -> Async<> {  //
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, co_await m.wait().setCancellationSlot(sig.slot()));
    }();

    coro.resume();
    TEST_ASSERT_FALSE(coro.done());  // blocked on lock()
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_TRUE(coro.done());  // released
}

TEST_F(t_event, wait_queue_cancelled) {
    CancellationSignal sig;

    auto make_coro_cancellable = [&]() -> Async<> {  //
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, co_await m.wait().setCancellationSlot(sig.slot()));
    };

    auto make_coro = [&]() -> Async<> {  //
        TEST_ASSERT_EQUAL(ErrCode::Success, co_await m.wait());
    };

    auto c1 = make_coro();
    auto c2 = make_coro_cancellable();
    auto c3 = make_coro();

    c1.resume();
    c2.resume();
    c3.resume();
    TEST_ASSERT_FALSE(c1.done());  // blocked
    TEST_ASSERT_FALSE(c2.done());  // blocked
    TEST_ASSERT_FALSE(c3.done());  // blocked

    sig.emit();
    TEST_ASSERT_FALSE(c1.done());  // blocked
    TEST_ASSERT_TRUE(c2.done());   // cancelled
    TEST_ASSERT_FALSE(c3.done());  // blocked

    m.fireOnce();
    TEST_ASSERT_TRUE(c1.done());
    TEST_ASSERT_TRUE(c3.done());
}

}  // namespace exec

TESTS_MAIN
