#include "coro/test.h"

#include <exec/Result.h>
#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/par/all.h>
#include <exec/coro/sync/Event.h>
#include <exec/coro/sync/Mutex.h>

#include <utest/utest.h>

namespace exec {

TEST_F(t_coro, wait_no_blocking) {
    Event e1, e2;
    e1.set();
    e2.set();

    auto coro = [](Event& e1, Event& e2) -> Async<> {  //
        auto [c1, c2] = co_await all(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
    };

    auto c = makeManualTask(coro(e1, e2));

    c.start();
    TEST_ASSERT_TRUE(c.done());
}

TEST_F(t_coro, wait_all_blocked) {
    Event e1, e2;

    auto coro = makeManualTask([](auto& e1, auto& e2) -> Async<> {  //
        auto [c1, c2] = co_await all(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
    }(e1, e2));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    SECTION("forward order") {
        e1.fireOnce();
        TEST_ASSERT_FALSE(coro.done());

        e2.fireOnce();
        TEST_ASSERT_TRUE(coro.done());
    }

    SECTION("backward order") {
        e1.fireOnce();
        TEST_ASSERT_FALSE(coro.done());

        e2.fireOnce();
        TEST_ASSERT_TRUE(coro.done());
    }
}

TEST_F(t_coro, wait_one_blocked) {
    Event e1, e2;
    e2.set();

    auto coro = makeManualTask([&](auto& e1, auto& e2) -> Async<> {  //
        auto [c1, c2] = co_await all(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
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
        co_await all(e1.wait(), e2.wait()).setCancellationSlot(slot);
    }(e1, e2, sig.slot()));

    coro.start();
    TEST_ASSERT_TRUE(sig.hasHandler());

    e1.fireOnce();
    e2.fireOnce();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, cancel_none_completed) {
    CancellationSignal sig;
    Event e1, e2;

    auto coro = makeManualTask([](auto& e1, auto& e2, auto slot) -> Async<> {
        auto [c1, c2] = co_await all(e1.wait(), e2.wait()).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(c1, ErrCode::Cancelled);
        TEST_ASSERT_EQUAL(c2, ErrCode::Cancelled);
    }(e1, e2, sig.slot()));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, cancel_partially_completed) {
    CancellationSignal sig;
    Event e1, e2;
    auto* set = GENERATE(&e1, &e2);
    set->set();

    auto coro = makeManualTask([](auto& e1, auto& e2, auto slot, auto set) -> Async<> {
        auto [c1, c2] = co_await all(e1.wait(), e2.wait()).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(c1, set == &e1 ? ErrCode::Success : ErrCode::Cancelled);
        TEST_ASSERT_EQUAL(c2, set == &e2 ? ErrCode::Success : ErrCode::Cancelled);
    }(e1, e2, sig.slot(), set));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, cancel_both_completed) {
    CancellationSignal sig;
    Event e1, e2;
    e1.set();
    e2.set();

    auto coro = makeManualTask([](auto& e1, auto& e2, auto slot) -> Async<> {
        auto [c1, c2] = co_await all(e1.wait(), e2.wait()).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
    }(e1, e2, sig.slot()));

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
}

TEST_F(t_coro, combine_all_with_all) {
    Event e1, e2, e3;

    auto coro = makeManualTask([&](auto& e1, auto& e2, auto& e3) -> Async<> {
        auto [c1, c2, a] = co_await all(e1.wait(), e2.wait(), all(e2.wait(), e3.wait()));
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
        auto&& [c3, c4] = a;
        TEST_ASSERT_EQUAL(c3, ErrCode::Success);
        TEST_ASSERT_EQUAL(c4, ErrCode::Success);
    }(e1, e2, e3));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());
    e2.fireOnce();
    TEST_ASSERT_FALSE(coro.done());
    e3.fireOnce();
    TEST_ASSERT_FALSE(coro.done());
    e1.fireOnce();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, combine_all_with_all_outer_cancelled) {
    CancellationSignal sig;
    Event e1, e2, e3;

    auto coro = makeManualTask([&](auto& e1, auto& e2, auto& e3, auto slot) -> Async<> {
        auto [c1, c2, a] =
            co_await all(e1.wait(), e2.wait(), all(e2.wait(), e3.wait())).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(c1, ErrCode::Cancelled);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
        auto&& [c3, c4] = a;
        TEST_ASSERT_EQUAL(c3, ErrCode::Success);
        TEST_ASSERT_EQUAL(c4, ErrCode::Success);
    }(e1, e2, e3, sig.slot()));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    e2.fireOnce();
    e3.fireOnce();
    TEST_ASSERT_FALSE(coro.done());

    sig.emit();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_coro, cancel_one_child_releases_other) {
    CancellationSignal sig;
    Mutex m;
    Event e;
    ErrCode wait_ec = ErrCode::Unknown;
    Result<Unit> cr;

    auto child = [](auto& m, auto& e, auto& wait_ec) -> Async<> {
        auto guard = co_await m.lock();
        wait_ec = co_await e.wait();
    };

    auto coro =
        makeManualTask([&](auto& m, auto& e, auto& wait_ec, auto& cr, auto slot) -> Async<> {  //
            auto [c, lg] = co_await all(child(m, e, wait_ec), m.lock()).setCancellationSlot(slot);
            cr = std::move(c);
        }(m, e, wait_ec, cr, sig.slot()));

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    SECTION("complete") {
        e.fireOnce();
        TEST_ASSERT_TRUE(coro.done());
        TEST_ASSERT_EQUAL(ErrCode::Success, wait_ec);
        TEST_ASSERT_TRUE(cr.hasValue());
    }

    SECTION("cancel child") {
        sig.emit();
        TEST_ASSERT_TRUE(coro.done());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, wait_ec);
        TEST_ASSERT_TRUE(cr.hasValue());
    }
}

}  // namespace exec

TESTS_MAIN
