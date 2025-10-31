#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/par/any.h>
#include <exec/coro/sync/Event.h>

#include <utest/utest.h>

namespace exec {

TEST(wait_no_blocking) {
    Event e1, e2;
    e1.set();
    e2.set();

    auto coro = makeManualTask([&]() -> Async<> {  //
        auto [c1, c2] = co_await any(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(ErrCode::Success, c1);
        TEST_ASSERT_EQUAL(ErrCode::Success, c2);
    }());

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
}

TEST(wait_any_blocked) {
    Event e1, e2;

    auto coro = makeManualTask([&]() -> Async<> {  //
        auto [c1, c2] = co_await any(e1.wait(), e2.wait());
        TEST_ASSERT_EQUAL(ErrCode::Success, c1);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c2);
    }());

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    e1.fireOnce();
    TEST_ASSERT_TRUE(coro.done());
}

TEST(connects_cancellation) {
    Event e1, e2;
    CancellationSignal sig;

    any(e1.wait(), e2.wait()).setCancellationSlot(sig.slot());
    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST(cancel_none_completed) {
    CancellationSignal sig;
    Event e1, e2;

    auto coro = makeManualTask([&]() -> Async<> {
        auto [c1, c2] = co_await any(e1.wait(), e2.wait()).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c1);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c2);
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
        auto [c1, c2] = co_await any(e1.wait(), e2.wait()).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(set == &e1 ? ErrCode::Success : ErrCode::Cancelled, c1);
        TEST_ASSERT_EQUAL(set == &e2 ? ErrCode::Success : ErrCode::Cancelled, c2);
    }());

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
}

TEST(cancel_both_completed) {
    CancellationSignal sig;
    Event e1, e2;
    e1.set();
    e2.set();

    auto coro = makeManualTask([&]() -> Async<> {
        auto [c1, c2] = co_await any(e1.wait(), e2.wait()).setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(ErrCode::Success, c1);
        TEST_ASSERT_EQUAL(ErrCode::Success, c2);
    }());

    coro.start();
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
}

TEST(combine_any_with_any_inner_cancelled) {
    CancellationSignal sig;
    Event e1, e2, e3;

    auto coro = makeManualTask([&]() -> Async<> {
        auto [c1, c2, a] = co_await any(
            e1.wait(), e2.wait(), any(e2.wait(), e3.wait()).setCancellationSlot(sig.slot()));
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c1);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c2);
        auto&& [c3, c4] = a;
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c3);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c4);
    }());

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    sig.emit();
    TEST_ASSERT_TRUE(coro.done());
}

TEST(combine_any_with_any_outer_cancelled) {
    CancellationSignal sig;
    Event e1, e2, e3;

    auto coro = makeManualTask([&]() -> Async<> {
        auto [c1, c2, a] = co_await any(e1.wait(), e2.wait(), any(e2.wait(), e3.wait()))
                               .setCancellationSlot(sig.slot());
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c1);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c2);
        auto&& [c3, c4] = a;
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c3);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c4);
    }());

    coro.start();
    TEST_ASSERT_FALSE(coro.done());

    sig.emit();
    TEST_ASSERT_TRUE(coro.done());
}

}  // namespace exec

TESTS_MAIN
