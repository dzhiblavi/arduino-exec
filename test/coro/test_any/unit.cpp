#include "coro/test.h"

#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/par/any.h>
#include <exec/coro/sync/Event.h>
#include <exec/coro/sync/Mutex.h>

#include <utest/utest.h>

namespace exec {

TEST_F(t_coro, wait_no_blocking) {
    Event e1, e2;
    e1.set();
    e2.set();

    auto coro = makeManualTask([](auto& e1, auto& e2) -> Async<> {  //
        auto [c1, c2] = co_await any(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(ErrCode::Success, c1);
        TEST_ASSERT_EQUAL(ErrCode::Success, c2);
    }(e1, e2));

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, wait_any_blocked) {
    Event e1, e2;

    auto coro = makeManualTask([](auto& e1, auto& e2) -> Async<> {  //
        auto [c1, c2] = co_await any(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(ErrCode::Success, c1);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c2);
    }(e1, e2));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    e1.fireOnce();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, connects_cancellation) {
    Event e1, e2;
    CancellationSignal sig;

    auto coro = makeManualTask([](auto& e1, auto& e2, auto slot) -> Async<> {
        co_await any(e1.wait(), e2.wait()).setCancellationSlot(slot);
    }(e1, e2, sig.slot()));

    coro.start();
    TEST_ASSERT_TRUE(sig.hasHandler());

    e1.fireOnce();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, cancel_none_completed) {
    CancellationSignal sig;
    Event e1, e2;

    auto coro = makeManualTask([](auto& e1, auto& e2, auto slot) -> Async<> {
        auto [c1, c2] = co_await any(e1.wait(), e2.wait()).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c1);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c2);
    }(e1, e2, sig.slot()));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());
    sig.emitSync();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, cancel_partially_completed) {
    CancellationSignal sig;
    Event e1, e2;
    auto* set = GENERATE(&e1, &e2);
    set->set();

    auto coro = makeManualTask([](auto& e1, auto& e2, auto slot, auto set) -> Async<> {
        auto [c1, c2] = co_await any(e1.wait(), e2.wait()).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(set == &e1 ? ErrCode::Success : ErrCode::Cancelled, c1);
        TEST_ASSERT_EQUAL(set == &e2 ? ErrCode::Success : ErrCode::Cancelled, c2);
    }(e1, e2, sig.slot(), set));

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_TRUE(std::noop_coroutine() == sig.emit());
}

TEST_F(t_coro, cancel_both_completed) {
    CancellationSignal sig;
    Event e1, e2;
    e1.set();
    e2.set();

    auto coro = makeManualTask([](auto& e1, auto& e2, auto slot) -> Async<> {
        auto [c1, c2] = co_await any(e1.wait(), e2.wait()).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(ErrCode::Success, c1);
        TEST_ASSERT_EQUAL(ErrCode::Success, c2);
    }(e1, e2, sig.slot()));

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_TRUE(std::noop_coroutine() == sig.emit());
}

TEST_F(t_coro, combine_any_with_any_outer_cancelled) {
    CancellationSignal sig;
    Event e1, e2, e3;

    auto coro = makeManualTask([&](auto& e1, auto& e2, auto& e3, auto slot) -> Async<> {
        auto [c1, c2, a] =
            co_await any(e1.wait(), e2.wait(), any(e2.wait(), e3.wait())).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c1);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c2);
        auto&& [c3, c4] = a;
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c3);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c4);
    }(e1, e2, e3, sig.slot()));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    sig.emitSync();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, cancel_one_child_releases_other) {
    CancellationSignal sig;
    Mutex m;
    Event e1, e2;
    ErrCode wait_ec = ErrCode::Unknown;
    Result<Unit> cr;

    auto child = [](auto& m, auto& e, auto& wait_ec) -> Async<> {
        auto guard = co_await m.lock();
        wait_ec = co_await e.wait();
    };

    auto coro = makeManualTask(
        [&](auto& m, auto& e1, auto& e2, auto& wait_ec, auto& cr, auto slot) -> Async<> {  //
            auto [c, _, lg] =
                co_await any(child(m, e1, wait_ec), e2.wait(), m.lock()).setCancellationSlot(slot);
            cr = std::move(c);
        }(m, e1, e2, wait_ec, cr, sig.slot()));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    SECTION("complete first and last") {
        e1.fireOnce();  // will complete child and m.lock()
        TEST_ASSERT_TRUE(coro.done());
        TEST_ASSERT_EQUAL(ErrCode::Success, wait_ec);
        TEST_ASSERT_TRUE(cr.hasValue());
    }

    SECTION("complete middle") {
        e2.fireOnce();  // will complete e2.wait()
        TEST_ASSERT_TRUE(coro.done());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, wait_ec);
        TEST_ASSERT_TRUE(cr.hasValue());
    }

    SECTION("cancel external") {
        sig.emitSync();
        TEST_ASSERT_TRUE(coro.done());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, wait_ec);
        TEST_ASSERT_TRUE(cr.hasValue());
    }
}

}  // namespace exec

TESTS_MAIN
