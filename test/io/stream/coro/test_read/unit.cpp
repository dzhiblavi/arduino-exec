#include "Executor.h"
#include "io/stream/TestStream.h"

#include <exec/Error.h>
#include <exec/coro/Async.h>
#include <exec/coro/par/any.h>
#include <exec/coro/wait.h>
#include <exec/executor/Executor.h>
#include <exec/io/stream/coro/read.h>
#include <exec/os/Service.h>

#include <supp/CircularBuffer.h>
#include <time/mono.h>

#include <utest/utest.h>

namespace exec {

struct t_read {
    t_read() {
        ttime::mono::set(ttime::Time(0));
        exec::setService<exec::Executor>(&executor);
    }

    template <size_t N>
    void push(const char (&s)[N]) {
        for (size_t i = 0; i < N - 1; ++i) {
            stream.buf.push(static_cast<int>(s[i]));
        }
    }

    template <size_t N>
    void check(const char (&s)[N], const char* dst) {
        for (size_t i = 0; i < N - 1; ++i) {
            TEST_ASSERT_EQUAL(s[i], dst[i]);  // NOLINT
        }
    }

    test::Stream stream;
    test::Executor executor;
    HeapTimerService<1> timerservice;
};

TEST_F(t_read, test_nonblocking) {
    auto coro = [&]() -> Async<> {
        char dst[20];
        size_t x = co_await read(&stream, dst, 4);
        TEST_ASSERT_EQUAL(4, x);
        check("abcd", dst);
    }();

    push("abcd");
    coro.resume();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_read, test_blocking) {
    auto coro = [&]() -> Async<> {
        char dst[20];
        size_t x = co_await read(&stream, dst, 4);
        TEST_ASSERT_EQUAL(4, x);
        check("abcd", dst);
    }();

    coro.resume();
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_EQUAL(1, executor.queued.size());

    push("ab");
    executor.queued.popFront()->runAll();
    TEST_ASSERT_FALSE(coro.done());
    TEST_ASSERT_EQUAL(1, executor.queued.size());

    push("cde");
    executor.queued.popFront()->runAll();
    TEST_ASSERT_TRUE(coro.done());
    TEST_ASSERT_TRUE(executor.queued.empty());
}

TEST_F(t_read, test_connects_cancellation) {
    CancellationSignal sig;
    char dst[20];
    [[maybe_unused]] auto&& op = read(&stream, dst, 4).setCancellationSlot(sig.slot());

    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST_F(t_read, test_cancelled) {
    CancellationSignal sig;

    auto coro = [this](CancellationSlot slot) -> Async<> {
        char dst[20];
        size_t x = co_await read(&stream, dst, 4).setCancellationSlot(slot);
        TEST_ASSERT_EQUAL(2, x);
    }(sig.slot());

    push("ab");

    coro.resume();  // block on read()
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
}

TEST_F(t_read, test_with_timeout_expired) {
    auto coro = [&]() -> Async<> {
        char dst[20];
        auto [rc, ec] = co_await any(read(&stream, dst, 4), wait(ttime::Duration(10)));
        TEST_ASSERT_EQUAL(3, rc);
        TEST_ASSERT_EQUAL(ErrCode::Success, ec);
        check("abc", dst);
    }();

    push("ab");
    coro.resume();
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(10, timerservice.wakeAt().micros());

    push("c");
    executor.queued.popFront()->runAll();
    TEST_ASSERT_EQUAL(1, executor.queued.size());

    ttime::mono::advance(ttime::Duration(10));
    timerservice.tick();  // should cancel read()
    TEST_ASSERT_FALSE(coro.done());

    executor.queued.popFront()->runAll();
    TEST_ASSERT_TRUE(coro.done());
}

TEST_F(t_read, test_with_timeout_succeeded) {
    auto coro = [&]() -> Async<> {
        char dst[20];
        auto [rc, ec] = co_await any(read(&stream, dst, 4), wait(ttime::Duration(10)));
        TEST_ASSERT_EQUAL(4, rc);
        TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);
        check("abcd", dst);
    }();

    push("ab");
    coro.resume();
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(10, timerservice.wakeAt().micros());

    push("cd");
    executor.queued.popFront()->runAll();  // should cancel timer
    TEST_ASSERT_TRUE(executor.queued.empty());
    TEST_ASSERT_EQUAL(ttime::Time::max().micros(), timerservice.wakeAt().micros());
    TEST_ASSERT_TRUE(coro.done());
}

}  // namespace exec

TESTS_MAIN
