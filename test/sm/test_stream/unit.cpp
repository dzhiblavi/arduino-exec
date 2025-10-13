#include "Executor.h"

#include <exec/executor/global.h>
#include <exec/sm/io/Stream.h>

#include <supp/CircularBuffer.h>

#include <utest/utest.h>

namespace exec {

struct TestStream : public ::Stream {
    virtual ~TestStream() = default;

    int available() override {
        return static_cast<int>(buf.size());
    }

    int read() override {
        return buf.empty() ? -1 : buf.pop();
    }

    int peek() override {
        return buf.front();
    }

    size_t write(uint8_t) override {
        TEST_FAIL();
        return 0;
    }

    supp::CircularBuffer<int, 10> buf;
};

struct t_stream {
    struct Task : Runnable {
        Task() = default;

        Runnable* run() override {
            return noop;
        }
    };

    t_stream() {
        exec::setExecutor(&executor);
    }

    template <size_t N>
    void push(const char (&s)[N]) {
        for (size_t i = 0; i < N - 1; ++i) {
            stream.buf.push(static_cast<int>(s[i]));
        }
    }

    template <size_t N>
    void check(const char (&s)[N]) {
        for (int i = 0; i < N - 1; ++i) {
            TEST_ASSERT_EQUAL(s[i], dst[i]);  // NOLINT
        }
    }

    char dst[20]{};
    ErrCode ec = ErrCode::Unknown;
    TestStream stream;
    Task t;
    Stream s{&stream};
    test::Executor executor;
};

TEST_F(t_stream, test_read_all_available) {
    push("abacaba");
    int left = 4;
    TEST_ASSERT_EQUAL(&t, s.read(dst, &left, &ec)(&t));
    TEST_ASSERT_EQUAL(ErrCode::Success, ec);
    TEST_ASSERT_EQUAL(0, left);
    check("abac");
}

TEST_F(t_stream, test_read_none_available) {
    int left = 4;
    TEST_ASSERT_EQUAL(noop, s.read(dst, &left, &ec)(&t));
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(&s, executor.queued.popFront());
    TEST_ASSERT_EQUAL(ErrCode::Unknown, ec);
    TEST_ASSERT_EQUAL(4, left);
}

TEST_F(t_stream, test_read_connects_cancellation) {
    CancellationSignal sig;
    int left = 4;
    s.read(dst, &left, &ec)(&t, sig.slot());
    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST_F(t_stream, test_read_none_cancelled) {
    CancellationSignal sig;
    int left = 4;
    s.read(dst, &left, &ec)(&t, sig.slot());
    TEST_ASSERT_EQUAL(&t, sig.emitRaw());
    TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);
    TEST_ASSERT_EQUAL(4, left);
    TEST_ASSERT_FALSE(sig.hasHandler());
}

TEST_F(t_stream, test_read_partial) {
    int left = 4;
    push("ab");
    TEST_ASSERT_EQUAL(noop, s.read(dst, &left, &ec)(&t));
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(&s, executor.queued.popFront());
    TEST_ASSERT_EQUAL(2, left);
    check("ab");
}

TEST_F(t_stream, test_read_partial_cancelled) {
    CancellationSignal sig;
    int left = 4;
    push("ab");
    s.read(dst, &left, &ec)(&t, sig.slot());
    TEST_ASSERT_EQUAL(&t, sig.emitRaw());
    TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);
    TEST_ASSERT_EQUAL(2, left);
    TEST_ASSERT_FALSE(sig.hasHandler());
    check("ab");
}

TEST_F(t_stream, test_read_multipart) {
    CancellationSignal sig;
    int left = 6;

    push("ab");
    s.read(dst, &left, &ec)(&t, sig.slot());
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    auto r = executor.queued.popFront();
    TEST_ASSERT_EQUAL(&s, r);
    TEST_ASSERT_EQUAL(4, left);

    push("ac");
    TEST_ASSERT_EQUAL(noop, r->run());
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    r = executor.queued.popFront();
    TEST_ASSERT_EQUAL(&s, r);
    TEST_ASSERT_EQUAL(2, left);

    push("ab");
    TEST_ASSERT_EQUAL(&t, r->run());
    TEST_ASSERT_TRUE(executor.queued.empty());
    TEST_ASSERT_EQUAL(0, left);

    check("abacab");
}

}  // namespace exec

TESTS_MAIN
