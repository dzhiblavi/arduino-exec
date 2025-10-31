#include "Executor.h"
#include "io/stream/TestPrint.h"

#include <exec/coro/Async.h>
#include <exec/coro/ManualTask.h>
#include <exec/coro/par/any.h>
#include <exec/coro/wait.h>
#include <exec/executor/Executor.h>
#include <exec/io/stream/coro/write.h>
#include <exec/os/Service.h>

#include <supp/CircularBuffer.h>

#include <utest/utest.h>

namespace exec {

struct t_write {
    t_write() {
        ttime::mono::set(ttime::Time(0));
        exec::setService<exec::Executor>(&executor);
    }

    template <size_t N>
    void check(const char (&s)[N]) {
        for (size_t i = 0; i < N - 1; ++i) {
            TEST_ASSERT_EQUAL_CHAR(s[i], print.buf[i]);
        }
    }

    void fill(size_t n) {
        for (size_t i = 0; i < n; ++i) {
            print.buf.push('x');
        }
    }

    void drain(size_t n) {
        for (size_t i = 0; i < n; ++i) {
            print.buf.pop();
        }
    }

    test::Print print;
    test::Executor executor;
    HeapTimerService<1> timerservice;
};

TEST_F(t_write, test_nonblocking) {
    auto coro = makeManualTask([&]() -> Async<> {
        const char* buf = "abacabax";
        size_t w = co_await write(&print, buf, 8);
        TEST_ASSERT_EQUAL(8, w);
    }());

    fill(2);
    coro.start();
    TEST_ASSERT_TRUE(coro.done());
    check("xxabacabax");
}

TEST_F(t_write, test_blocking) {
    auto coro = makeManualTask([&]() -> Async<> {
        const char* buf = "abacabax";
        size_t w = co_await write(&print, buf, 8);
        TEST_ASSERT_EQUAL(8, w);
    }());

    fill(8);
    coro.start();  // ab
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_EQUAL(1, executor.queued.size());

    drain(4);
    executor.queued.popFront()->runAll();  // acab
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_EQUAL(1, executor.queued.size());

    drain(2);
    executor.queued.popFront()->runAll();  // ax
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_TRUE(executor.queued.empty());

    check("xxabacabax");
}

TEST_F(t_write, test_connects_cancellation) {
    CancellationSignal sig;
    auto buf = "abacaba";
    [[maybe_unused]] auto&& op = write(&print, buf, 4).setCancellationSlot(sig.slot());

    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST_F(t_write, test_cancelled) {
    CancellationSignal sig;

    auto coro = makeManualTask([this](CancellationSlot slot) -> Async<> {
        auto buf = "abcd";
        size_t x = co_await write(&print, buf, 4).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(2, x);
    }(sig.slot()));

    fill(8);

    coro.start();  // block on write()
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_TRUE(sig.hasHandler());
    TEST_ASSERT_EQUAL(1, executor.queued.size());

    executor.queued.popFront()->runAll();
    TEST_ASSERT_FALSE(coro.done());

    sig.emit();  // should not unblock
    TEST_ASSERT_FALSE(sig.hasHandler());
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_EQUAL(1, executor.queued.size());

    executor.queued.popFront()->runAll();  // this should finish the read()
    TEST_ASSERT_TRUE(coro.done());

    check("xxxxxxxxab");
}

TEST_F(t_write, test_with_timeout_expired) {
    auto coro = makeManualTask([&]() -> Async<> {
        auto buf = "abcd";
        auto [rc, ec] = co_await any(write(&print, buf, 4), wait(ttime::Duration(10)));
        TEST_ASSERT_EQUAL(3, rc);
        TEST_ASSERT_EQUAL(ErrCode::Success, ec);
    }());

    fill(8);
    coro.start();
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(10, timerservice.wakeAt().micros());

    drain(1);
    executor.queued.popFront()->runAll();
    TEST_ASSERT_EQUAL(1, executor.queued.size());

    ttime::mono::advance(ttime::Duration(10));
    timerservice.tick();  // should cancel read()
    TEST_ASSERT_FALSE(coro.done());

    executor.queued.popFront()->runAll();
    TEST_ASSERT_TRUE(coro.done());

    check("xxxxxxxabc");
}

TEST_F(t_write, test_with_timeout_succeeded) {
    auto coro = makeManualTask([&]() -> Async<> {
        auto buf = "abcd";
        auto [rc, ec] = co_await any(write(&print, buf, 4), wait(ttime::Duration(10)));
        TEST_ASSERT_EQUAL(4, rc);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);
    }());

    fill(8);
    coro.start();
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(10, timerservice.wakeAt().micros());

    drain(2);
    executor.queued.popFront()->runAll();  // should cancel timer
    TEST_ASSERT_TRUE(executor.queued.empty());
    TEST_ASSERT_EQUAL(ttime::Time::max().micros(), timerservice.wakeAt().micros());
    TEST_ASSERT_TRUE(coro.done());

    check("xxxxxxabcd");
}

}  // namespace exec

TESTS_MAIN
