#include "Executor.h"

#include <exec/io/sm/Interrupt.h>
#include <exec/sys/int/interrupts.h>

#include <utest/utest.h>

namespace exec {

struct t_interrupt {
    struct Task : Runnable {
        Task() = default;

        Runnable* run() override {
            return noop;
        }
    };

    t_interrupt() {
        exec::setExecutor(&executor);
        i.begin();
    }

    ErrCode ec = ErrCode::Unknown;
    Task t;
    test::Executor executor;
    exec::Interrupt<1, exec::InterruptMode::Change> i;
};

TEST_F(t_interrupt, test_blocks_no_interrupt) {
    TEST_ASSERT_EQUAL(noop, i.wait(&ec)(&t));
    i.tick();
    TEST_ASSERT_TRUE(executor.queued.empty());
    TEST_ASSERT_EQUAL(ErrCode::Unknown, ec);
}

TEST_F(t_interrupt, test_does_not_block_after_interrupt) {
    raiseInterrupt(1, InterruptMode::Change);
    TEST_ASSERT_EQUAL(&t, i.wait(&ec)(&t));
    TEST_ASSERT_TRUE(executor.queued.empty());
    TEST_ASSERT_EQUAL(ErrCode::Success, ec);
}

TEST_F(t_interrupt, test_end_ignores_interrupts) {
    i.end();
    raiseInterrupt(1, InterruptMode::Change);
    TEST_ASSERT_EQUAL(noop, i.wait(&ec)(&t));
    TEST_ASSERT_TRUE(executor.queued.empty());
    TEST_ASSERT_EQUAL(ErrCode::Unknown, ec);
}

TEST_F(t_interrupt, test_unblocks_on_interrupt) {
    i.wait (&ec)(&t);
    raiseInterrupt(1, InterruptMode::Change);

    i.tick();
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(&t, executor.queued.front());
    TEST_ASSERT_EQUAL(ErrCode::Success, ec);
}

TEST_F(t_interrupt, test_connects_cancellation) {
    CancellationSignal sig;
    i.wait (&ec)(&t, sig.slot());
    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST_F(t_interrupt, test_cancelled) {
    CancellationSignal sig;
    i.wait (&ec)(&t, sig.slot());

    TEST_ASSERT_EQUAL(&t, sig.emitRaw());
    TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);

    i.tick();
    TEST_ASSERT_TRUE(executor.queued.empty());
}

int called = 0;

TEST_F(t_interrupt, test_calls_isr) {
    called = 0;
    i.setISR([] { ++called; });
    raiseInterrupt(1, InterruptMode::Change);
    raiseInterrupt(0, InterruptMode::Change);
    raiseInterrupt(1, InterruptMode::Falling);
    raiseInterrupt(1, InterruptMode::Change);
    TEST_ASSERT_EQUAL(2, called);
}

TEST_F(t_interrupt, test_correct_interrupt_no) {
    called = 0;
    i.setISR([] { ++called; });
    raiseInterrupt(1, InterruptMode::Change);
    raiseInterrupt(0, InterruptMode::Change);
    TEST_ASSERT_EQUAL(1, called);
}

TEST_F(t_interrupt, test_correct_interrupt_mode) {
    called = 0;
    i.setISR([] { ++called; });
    raiseInterrupt(1, InterruptMode::Change);
    raiseInterrupt(1, InterruptMode::Falling);
    raiseInterrupt(1, InterruptMode::Rising);
    TEST_ASSERT_EQUAL(1, called);
}

}  // namespace exec

TESTS_MAIN
