#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/par/all.h>
#include <exec/coro/sync/Event.h>
#include <exec/Result.h>

#include <utest/utest.h>

namespace exec {

TEST(wait_no_blocking) {
    Event e1, e2;
    e1.set();
    e2.set();

    auto coro = makeManualTask([&]() -> Async<> {  //
        auto [c1, c2] = co_await all(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
    }());

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
}

TEST(wait_all_blocked) {
    Event e1, e2;

    auto coro = makeManualTask([&]() -> Async<> {  //
        auto [c1, c2] = co_await all(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
    }());

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

TEST(wait_one_blocked) {
    Event e1, e2;
    e2.set();

    auto coro = makeManualTask([&]() -> Async<> {  //
        auto [c1, c2] = co_await all(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
    }());

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    e1.fireOnce();
    TEST_ASSERT_TRUE(coro.done());
}

TEST(connects_cancellation) {
    Event e1, e2;
    CancellationSignal sig;

    auto coro = makeManualTask([&]() -> Async<> {
        co_await all(e1.wait(), e2.wait()).setCancellationSlot(sig.slot());
    }());

    coro.start();
    TEST_ASSERT_TRUE(sig.hasHandler());

    e1.fireOnce();
    e2.fireOnce();
    TEST_ASSERT_TRUE(coro.done());
}

TEST(cancel_none_completed) {
    CancellationSignal sig;
    Event e1, e2;

    auto coro = makeManualTask([&]() -> Async<> {
        auto [c1, c2] = co_await all(e1.wait(), e2.wait()).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(c1, ErrCode::Cancelled);
        TEST_ASSERT_EQUAL(c2, ErrCode::Cancelled);
    }());

    coro.start();
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_TRUE(coro.done());
}

TEST(cancel_partially_completed) {
    CancellationSignal sig;
    Event e1, e2;
    auto* set = GENERATE(&e1, &e2);
    set->set();

    auto coro = makeManualTask([&]() -> Async<> {
        auto [c1, c2] = co_await all(e1.wait(), e2.wait()).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(c1, set == &e1 ? ErrCode::Success : ErrCode::Cancelled);
        TEST_ASSERT_EQUAL(c2, set == &e2 ? ErrCode::Success : ErrCode::Cancelled);
    }());

    coro.start();
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_TRUE(coro.done());
}

TEST(cancel_both_completed) {
    CancellationSignal sig;
    Event e1, e2;
    e1.set();
    e2.set();

    auto coro = makeManualTask([&]() -> Async<> {
        auto [c1, c2] = co_await all(e1.wait(), e2.wait()).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
    }());

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
}

TEST(combine_all_with_all) {
    Event e1, e2, e3;

    auto coro = makeManualTask([&]() -> Async<> {
        auto [c1, c2, a] = co_await all(e1.wait(), e2.wait(), all(e2.wait(), e3.wait()));
        TEST_ASSERT_EQUAL(c1, ErrCode::Success);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
        auto&& [c3, c4] = a;
        TEST_ASSERT_EQUAL(c3, ErrCode::Success);
        TEST_ASSERT_EQUAL(c4, ErrCode::Success);
    }());

    coro.start();
    TEST_ASSERT_FALSE(coro.done());
    e2.fireOnce();
    TEST_ASSERT_FALSE(coro.done());
    e3.fireOnce();
    TEST_ASSERT_FALSE(coro.done());
    e1.fireOnce();
    TEST_ASSERT_TRUE(coro.done());
}

TEST(combine_all_with_all_outer_cancelled) {
    CancellationSignal sig;
    Event e1, e2, e3;

    auto coro = makeManualTask([&]() -> Async<> {
        auto [c1, c2, a] = co_await all(e1.wait(), e2.wait(), all(e2.wait(), e3.wait()))
                               .setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(c1, ErrCode::Cancelled);
        TEST_ASSERT_EQUAL(c2, ErrCode::Success);
        auto&& [c3, c4] = a;
        TEST_ASSERT_EQUAL(c3, ErrCode::Success);
        TEST_ASSERT_EQUAL(c4, ErrCode::Success);
    }());

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    e2.fireOnce();
    e3.fireOnce();
    TEST_ASSERT_FALSE(coro.done());

    sig.emit();
    TEST_ASSERT_TRUE(coro.done());
}

}  // namespace exec

TESTS_MAIN
