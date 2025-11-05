#include "coro/test.h"

#include <exec/coro/Async.h>
#include <exec/coro/DynamicScope.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/sync/Event.h>

#include <utest/utest.h>

namespace exec {

TEST_F(t_coro, join_empty) {
    auto coro = [&]() -> Async<> {
        DynamicScope scope;
        co_await scope.join();
    };

    auto m = makeManualTask(coro());

    m.start();
    TEST_ASSERT_TRUE(m.done());
}

TEST_F(t_coro, join_sync) {
    Event e;

    auto coro = [&]() -> Async<> {
        DynamicScope scope;

        scope.add(e.wait());
        scope.add(e.wait());
        scope.add(e.wait());

        co_await scope.join();
    };

    auto m = makeManualTask(coro());

    e.set();
    m.start();
    TEST_ASSERT_TRUE(m.done());
}

TEST_F(t_coro, join_tasks) {
    Event e;

    auto coro = [&]() -> Async<> {
        DynamicScope scope;

        scope.add(e.wait());
        scope.add(e.wait());
        scope.add(e.wait());

        co_await scope.join();
    };

    auto m = makeManualTask(coro());

    m.start();
    TEST_ASSERT_FALSE(m.done());

    e.fireOnce();
    TEST_ASSERT_TRUE(m.done());
}

TEST_F(t_coro, cancel_join) {
    CancellationSignal sig;
    Event e;

    auto coro = [&]() -> Async<> {
        DynamicScope scope;

        scope.add(e.wait());
        scope.add(e.wait());
        scope.add(e.wait());

        co_await scope.join().setCancellationSlot(sig.slot());
    };

    auto m = makeManualTask(coro());

    m.start();
    TEST_ASSERT_FALSE(m.done());

    sig.emit();
    TEST_ASSERT_TRUE(m.done());
    TEST_ASSERT_FALSE(sig.hasHandler());
}

TEST_F(t_coro, cancel_partial_complete) {
    CancellationSignal sig;
    Event e1, e2;

    auto coro = [&]() -> Async<> {
        DynamicScope scope;

        scope.add(e1.wait());
        scope.add(e2.wait());

        co_await scope.join().setCancellationSlot(sig.slot());
    };

    auto m = makeManualTask(coro());

    m.start();
    TEST_ASSERT_FALSE(m.done());

    SECTION("complete first") {
        e1.fireOnce();
    }

    SECTION("complete second") {
        e2.fireOnce();
    }

    TEST_ASSERT_FALSE(m.done());
    sig.emit();
    TEST_ASSERT_TRUE(m.done());
    TEST_ASSERT_FALSE(sig.hasHandler());
}

TEST_F(t_coro, drop_join) {
    CancellationSignal sig;
    Event e1, e2;

    auto coro = [&]() -> Async<> {
        co_await e1.wait();

        DynamicScope scope;
        scope.add(e2.wait());
        co_await scope.join();
    };

    auto m = makeManualTask(coro().setCancellationSlot(sig.slot()));

    m.start();
    TEST_ASSERT_FALSE(m.done());

    sig.emit();
    TEST_ASSERT_TRUE(m.done());
}

TEST_F(t_coro, add_while_joining) {
    DynamicScope scope;
    Event e1, e2;

    auto coro = [&]() -> Async<> {
        scope.add(e1.wait());
        scope.add(e1.wait());
        scope.add(e1.wait());
        co_await scope.join();
    };

    auto add = [&]() -> Async<> {
        scope.add(e2.wait());
        scope.add(e2.wait());
        co_return;
    };

    auto m = makeManualTask(coro());
    auto a = makeManualTask(add());

    m.start();
    TEST_ASSERT_FALSE(m.done());
    TEST_ASSERT_EQUAL(3, scope.size());

    a.start();
    TEST_ASSERT_TRUE(a.done());
    TEST_ASSERT_EQUAL(5, scope.size());

    e1.fireOnce();
    TEST_ASSERT_EQUAL(2, scope.size());
    TEST_ASSERT_FALSE(m.done());

    e2.fireOnce();
    TEST_ASSERT_EQUAL(0, scope.size());
    TEST_ASSERT_TRUE(m.done());
}

}  // namespace exec

TESTS_MAIN
