#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/sync/Event.h>

#include <utest/utest.h>

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
        Unit u = co_await task();
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
        int x = co_await task();
        TEST_ASSERT_EQUAL(239, x);
        done = true;
    };

    auto m = makeManualTask(main());

    m.start();
    TEST_ASSERT_TRUE(m.done());
    TEST_ASSERT_TRUE(done);
}

TEST(async_sync_does_not_connect_cancellation) {
    CancellationSignal sig;

    auto make_task = [&]() -> Async<int> {  //
        co_return 239;
    };

    auto task = makeManualTask(make_task().setCancellationSlot(sig.slot()));
    task.start();

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
    sig.emit();    // will cancel event.wait()
    TEST_ASSERT_TRUE(task.done());
}

TEST(async_extract_cancellation_slot_with_signal) {
    CancellationSignal sig;

    auto make_task = [&]() -> Async<> {  //
        auto slot = co_await extract_cancellation_slot;
        TEST_ASSERT_TRUE(slot.isConnected());
    };

    auto task = makeManualTask(make_task().setCancellationSlot(sig.slot()));
    task.start();
    TEST_ASSERT_TRUE(task.done());
}

TEST(async_extract_cancellation_slot_without_signal) {
    auto make_task = [&]() -> Async<> {  //
        auto slot = co_await extract_cancellation_slot;
        TEST_ASSERT_FALSE(slot.isConnected());
    };

    auto task = makeManualTask(make_task());
    task.start();
    TEST_ASSERT_TRUE(task.done());
}

TEST(async_no_automatic_cancellation_when_slot_is_extracted) {
    Event event;
    CancellationSignal sig;

    auto make_task = [&]() -> Async<> {  //
        [[maybe_unused]] auto slot = co_await extract_cancellation_slot;
        co_await event.wait();
    };
    auto task = makeManualTask(make_task().setCancellationSlot(sig.slot()));

    sig.emit();  // not yet started
    TEST_ASSERT_FALSE(task.done());

    task.start();  // installs (missing) cancellation and blocks
    sig.emit();    // will do nothing
    TEST_ASSERT_FALSE(task.done());

    event.fireOnce();
    TEST_ASSERT_TRUE(task.done());
}

TEST(async_replace_cancellation_slot) {
    Event event;
    CancellationSignal sig1;
    CancellationSignal sig2;
    ErrCode expected = ErrCode::Unknown;

    auto make_task = [&]() -> Async<> {  //
        [[maybe_unused]] auto extracted = co_await replace_cancellation_slot{sig2.slot()};
        auto c = co_await event.wait();
        TEST_ASSERT_EQUAL(expected, c);
    };
    auto task = makeManualTask(make_task().setCancellationSlot(sig1.slot()));

    task.start();  // installs sig2 cancellation and blocks
    sig1.emit();   // will do nothing
    TEST_ASSERT_FALSE(task.done());

    SECTION("fire still completes") {
        expected = ErrCode::Success;
        event.fireOnce();
        TEST_ASSERT_TRUE(task.done());
    }

    SECTION("cancell through new slot") {
        expected = ErrCode::Cancelled;
        sig2.emit();
        TEST_ASSERT_TRUE(task.done());
    }
}

TEST(async_use_extracted_slot_manually) {
    Event event;
    CancellationSignal sig;
    ErrCode expected1 = ErrCode::Unknown;
    ErrCode expected2 = ErrCode::Unknown;

    auto make_task = [&]() -> Async<> {  //
        auto slot = co_await extract_cancellation_slot;

        // wait with no cancellation
        auto c1 = co_await event.wait();
        TEST_ASSERT_EQUAL(expected1, c1);

        // wait with manually attached cancellation
        auto c2 = co_await event.wait().setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(expected2, c2);
    };
    auto task = makeManualTask(make_task().setCancellationSlot(sig.slot()));

    task.start();  // blocks on uncancellable event.wait()
    sig.emit();    // will do nothing (otherwise test fails due to Unknown expected code)
    TEST_ASSERT_FALSE(task.done());

    SECTION("both complete") {
        expected1 = ErrCode::Success;
        expected2 = ErrCode::Success;

        event.fireOnce();
        TEST_ASSERT_FALSE(task.done());

        event.fireOnce();
        TEST_ASSERT_TRUE(task.done());
    }

    SECTION("second operation is cancelled") {
        expected1 = ErrCode::Success;
        expected2 = ErrCode::Cancelled;

        event.fireOnce();  // release the uncancellable wait
        TEST_ASSERT_FALSE(task.done());

        sig.emit();  // now the slot is installed into the second wait
        TEST_ASSERT_TRUE(task.done());
    }
}

TEST(async_chain_cancellation) {
    Event event;
    CancellationSignal sig;
    ErrCode expected = ErrCode::Unknown;

    auto task = [&]() -> Async<ErrCode> {  //
        co_return co_await event.wait();
    };

    auto main = [&]() -> Async<> {
        auto c = co_await task();
        TEST_ASSERT_EQUAL(expected, c);
    };

    auto t = makeManualTask(main().setCancellationSlot(sig.slot()));

    t.start();
    TEST_ASSERT_FALSE(t.done());  // task is blocked on event.wait()

    SECTION("no cancellation") {
        expected = ErrCode::Success;
        event.fireOnce();
        TEST_ASSERT_TRUE(t.done());
    }

    SECTION("inner operation is cancelled") {
        expected = ErrCode::Cancelled;
        sig.emit();
        TEST_ASSERT_TRUE(t.done());
    }
}

}  // namespace exec

TESTS_MAIN
