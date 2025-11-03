#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/sync/Event.h>
#include <exec/sys/memory/usage.h>

#include <utest/utest.h>

bool fail_allocation = false;

namespace exec::alloc {

void* allocate(size_t size, const std::nothrow_t&) noexcept {
    return fail_allocation ? nullptr : malloc(size);
}

void deallocate(void* ptr, size_t) noexcept {
    free(ptr);
}

}  // namespace exec::alloc

namespace exec {

TEST(async_initial_suspend) {
    bool done = false;

    auto coro = [&]() -> Async<> {
        done = true;
        co_return;
    };

    auto m = makeManualTask(coro());

    TEST_ASSERT_FALSE(m.done());
    TEST_ASSERT_FALSE(done);

    m.start();

    TEST_ASSERT_TRUE(m.done());
    TEST_ASSERT_TRUE(done);
}

TEST(async_co_return_void) {
    bool done = false;

    auto task = [&]() -> Async<> {  //
        co_return;
    };

    auto main = [&]() -> Async<> {
        auto u = co_await task();
        (void)u;
        done = true;
    };

    auto m = makeManualTask(main());

    m.start();
    TEST_ASSERT_TRUE(m.done());
    TEST_ASSERT_TRUE(done);
}

TEST(async_co_return_value) {
    bool done = false;

    auto task = [&]() -> Async<int> {  //
        co_return 239;
    };

    auto main = [&]() -> Async<> {
        auto x = co_await task();
        TEST_ASSERT_TRUE(x.hasValue());
        TEST_ASSERT_EQUAL(239, *x);
        done = true;
    };

    auto m = makeManualTask(main());

    m.start();
    TEST_ASSERT_TRUE(m.done());
    TEST_ASSERT_TRUE(done);
}

TEST(async_cancellation) {
    Event e;
    CancellationSignal sig;

    auto child = [&]() -> Async<Unit> {  //
        auto ec = co_await e.wait();
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);
    };

    auto parent = [&]() -> Async<int> {
        auto ec = co_await child();

        // success because no co_awaits after cancelled operation
        TEST_ASSERT_EQUAL(ErrCode::Success, ec.code());

        ec = co_await child();  // will finalize here
        TEST_FAIL_MESSAGE("should've not reached this point");

        co_return 10;
    };

    auto task = makeManualTask(parent().setCancellationSlot(sig.slot()));

    task.start();
    TEST_ASSERT_FALSE(task.done());

    sig.emit();
    TEST_ASSERT_TRUE(task.done());
}

TEST(async_cancellation_ignored) {
    Event e;
    CancellationSignal sig;

    auto child = [&]() -> Async<Unit> {  //
        auto ec = co_await e.wait();
        TEST_ASSERT_EQUAL(ErrCode::Success, ec);
    };

    auto parent = [&]() -> Async<> {
        auto guard = co_await ignore_cancellation;

        auto ec = co_await child();
        TEST_ASSERT_EQUAL(ErrCode::Success, ec.code());
    };

    auto task = makeManualTask(parent().setCancellationSlot(sig.slot()));

    task.start();
    TEST_ASSERT_FALSE(task.done());

    sig.emit();
    TEST_ASSERT_FALSE(sig.hasHandler());
    TEST_ASSERT_FALSE(task.done());

    e.fireOnce();
    TEST_ASSERT_TRUE(task.done());
}

TEST(async_sync_does_not_connect_cancellation) {
    CancellationSignal sig;

    auto make_task = [&]() -> Async<int> {  //
        co_return 239;
    };

    auto task = makeManualTask(make_task().setCancellationSlot(sig.slot()));
    task.start();

    TEST_ASSERT_TRUE(task.done());
    TEST_ASSERT_FALSE(sig.hasHandler());
}

TEST(async_async_connects_cancellation) {
    Event event;
    CancellationSignal sig;

    auto make_task = [&]() -> Async<> {  //
        co_await event.wait();
    };
    auto task = makeManualTask(make_task().setCancellationSlot(sig.slot()));

    TEST_ASSERT_FALSE(sig.hasHandler());
    task.start();  // installs cancellation and blocks
    TEST_ASSERT_TRUE(sig.hasHandler());

    event.fireOnce();
    TEST_ASSERT_TRUE(task.done());
}

TEST(async_cancel_while_blocked) {
    Event event;
    CancellationSignal sig;

    auto make_task = [&]() -> Async<> {  //
        co_await event.wait();
    };
    auto task = makeManualTask(make_task().setCancellationSlot(sig.slot()));

    sig.emit();  // not yet started
    TEST_ASSERT_FALSE(task.done());

    task.start();  // installs cancellation and blocks
    TEST_ASSERT_FALSE(task.done());
    sig.emit();  // will cancel event.wait()
    TEST_ASSERT_TRUE(task.done());
}

TEST(async_chain_cancellation) {
    Event event;
    CancellationSignal sig;

    auto task = [&]() -> Async<> {  //
        co_await event.wait();
    };

    auto main = [&]() -> Async<> {
        auto c = co_await task();

        // success in either case because no co_awaits follow the cancelled operation
        TEST_ASSERT_EQUAL(ErrCode::Success, c.code());
    };

    auto t = makeManualTask(main().setCancellationSlot(sig.slot()));

    t.start();
    TEST_ASSERT_FALSE(t.done());  // task is blocked on event.wait()

    SECTION("no cancellation") {
        event.fireOnce();
        TEST_ASSERT_TRUE(t.done());
    }

    SECTION("inner operation is cancelled") {
        sig.emit();
        TEST_ASSERT_TRUE(t.done());
    }
}

TEST(async_allocation_failure) {
    Event event;

    bool done = false;

    auto task = [&]() -> Async<int> {  //
        co_return 10;
    };
    auto main = [&]() -> Async<> {  //
        LINFO("call");
        auto x = co_await task();
        LINFO("call end");
        TEST_ASSERT_EQUAL(ErrCode::OutOfMemory, x.code());
        done = true;
    };

    auto t = makeManualTask(main());

    fail_allocation = true;
    t.start();
    TEST_ASSERT_TRUE(done);
    TEST_ASSERT_TRUE(t.done());
    fail_allocation = false;
}

TEST(async_multiple_suspension_points) {
    Event event;
    CancellationSignal sig;
    int i = 0;

    auto main = [&]() -> Async<> {
        i = 1;
        ErrCode c{};

        c = co_await event.wait();
        i = 2;
        TEST_ASSERT_EQUAL(ErrCode::Success, c);

        c = co_await event.wait();  // cancel
        i = 3;
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, c);

        c = co_await event.wait();
        i = 4;
        TEST_FAIL_MESSAGE("should not be reached");
    };

    auto t = makeManualTask(main().setCancellationSlot(sig.slot()));
    t.start();

    event.fireOnce();
    sig.emit();

    TEST_ASSERT_TRUE(t.done());
    TEST_ASSERT_EQUAL(3, i);
}

}  // namespace exec

TESTS_MAIN
