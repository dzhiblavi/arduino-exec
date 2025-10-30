#include "Executor.h"

#include <exec/executor/Executor.h>
#include <exec/io/stream/sm/AsyncStreamWrite.h>
#include <exec/os/Service.h>

#include <supp/CircularBuffer.h>

#include <utest/utest.h>

namespace exec {

struct TestPrint : public Print {
    virtual ~TestPrint() = default;

    int availableForWrite() override {
        return static_cast<int>(buf.capacity() - buf.size());
    }

    size_t write(const char* b, size_t size) override {
        size = std::min(size, buf.capacity() - buf.size());
        for (size_t i = 0; i < size; ++i) {
            buf.push(b[i]);  // NOLINT
        }
        return size;
    }

    supp::CircularBuffer<char, 10> buf;
};

struct t_async_print_write {
    struct Task : Runnable {
        Task() = default;

        Runnable* run() override {
            return noop;
        }
    };

    t_async_print_write() {
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

    ErrCode ec = ErrCode::Unknown;
    TestPrint print;
    Task t;
    AsyncPrintWrite s{&print};
    test::Executor executor;
};

TEST_F(t_async_print_write, test_nonblocking) {
    fill(2);
    int left = 8;
    auto buf = "abacabax";
    TEST_ASSERT_EQUAL(&t, s(buf, &left, &ec)(&t));
    TEST_ASSERT_EQUAL(ErrCode::Success, ec);
    TEST_ASSERT_EQUAL(0, left);
    check("xxabacabax");
}

TEST_F(t_async_print_write, test_buffer_filled) {
    fill(10);
    int left = 1;
    auto buf = "a";
    TEST_ASSERT_EQUAL(noop, s(buf, &left, &ec)(&t));
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(&s, executor.queued.popFront());
    TEST_ASSERT_EQUAL(ErrCode::Unknown, ec);
    TEST_ASSERT_EQUAL(1, left);
}

TEST_F(t_async_print_write, test_connects_cancellation) {
    CancellationSignal sig;
    int left = 4;
    auto buf = "abcd";
    fill(10);
    s(buf, &left, &ec)(&t, sig.slot());
    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST_F(t_async_print_write, test_none_cancelled) {
    CancellationSignal sig;
    int left = 4;
    auto buf = "abcd";
    fill(10);
    s(buf, &left, &ec)(&t, sig.slot());
    TEST_ASSERT_EQUAL(&t, sig.emitRaw());
    TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);
    TEST_ASSERT_EQUAL(4, left);
    TEST_ASSERT_FALSE(sig.hasHandler());
}

TEST_F(t_async_print_write, test_partial_write) {
    int left = 4;
    auto buf = "abcd";
    fill(8);
    TEST_ASSERT_EQUAL(noop, s(buf, &left, &ec)(&t));
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(&s, executor.queued.front());
    TEST_ASSERT_EQUAL(2, left);
    check("xxxxxxxxab");

    drain(1);
    TEST_ASSERT_EQUAL(noop, executor.queued.popFront()->run());
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(&s, executor.queued.front());
    TEST_ASSERT_EQUAL(1, left);
    check("xxxxxxxabc");

    drain(1);
    TEST_ASSERT_EQUAL(&t, executor.queued.popFront()->run());
    TEST_ASSERT_TRUE(executor.queued.empty());
    TEST_ASSERT_EQUAL(0, left);
    check("xxxxxxabcd");
}

TEST_F(t_async_print_write, test_partial_write_cancelled) {
    CancellationSignal sig;
    int left = 4;
    auto buf = "abcd";
    fill(8);
    s(buf, &left, &ec)(&t, sig.slot());
    TEST_ASSERT_EQUAL(&t, sig.emitRaw());
    TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);
    TEST_ASSERT_FALSE(sig.hasHandler());
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(&s, executor.queued.front());
    TEST_ASSERT_EQUAL(noop, executor.queued.popFront()->run());

    TEST_ASSERT_EQUAL(2, left);
    check("xxxxxxxxab");
}

}  // namespace exec

TESTS_MAIN
