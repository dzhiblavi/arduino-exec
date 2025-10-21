#include <exec/io/int/interrupts.h>
#include <exec/io/int/sm/Interrupt.h>

#include <utest/utest.h>

namespace exec {

struct t_interrupt {
    struct Task : Runnable {
        Task(int& c) : cnt(c) {}

        Runnable* run() override {
            ++cnt;
            return noop;
        }

        int& cnt;
    };

    t_interrupt() {
        i.begin();
    }

    int cnt = 0;
    Task t{cnt};
    ErrCode ec = ErrCode::Unknown;
    exec::Interrupt<1, exec::InterruptMode::Change> i;
};

TEST_F(t_interrupt, test_blocks_no_interrupt) {
    TEST_ASSERT_EQUAL(noop, i.wait(&ec)(&t));
    i.tick();
    TEST_ASSERT_EQUAL(0, cnt);
    TEST_ASSERT_EQUAL(ErrCode::Unknown, ec);
}

TEST_F(t_interrupt, test_does_not_block_after_interrupt) {
    raiseInterrupt(1, InterruptMode::Change);
    TEST_ASSERT_EQUAL(&t, i.wait(&ec)(&t));
    TEST_ASSERT_EQUAL(0, cnt);
    TEST_ASSERT_EQUAL(ErrCode::Success, ec);
}

TEST_F(t_interrupt, test_end_ignores_interrupts) {
    i.end();
    raiseInterrupt(1, InterruptMode::Change);
    TEST_ASSERT_EQUAL(noop, i.wait(&ec)(&t));
    TEST_ASSERT_EQUAL(0, cnt);
    TEST_ASSERT_EQUAL(ErrCode::Unknown, ec);
}

TEST_F(t_interrupt, test_unblocks_on_interrupt) {
    i.wait (&ec)(&t);
    raiseInterrupt(1, InterruptMode::Change);

    i.tick();
    TEST_ASSERT_EQUAL(1, cnt);
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
    TEST_ASSERT_EQUAL(0, cnt);
}

TEST_F(t_interrupt, test_wake_at) {
    SECTION("no pending operations") {
        TEST_ASSERT_EQUAL(ttime::Time::max().micros(), i.wakeAt().micros());
    }

    SECTION("pending, not fired") {
        i.wait (&ec)(&t);
        TEST_ASSERT_EQUAL(ttime::Time::max().micros(), i.wakeAt().micros());
    }

    SECTION("pending, fired") {
        i.wait (&ec)(&t);
        raiseInterrupt(1, InterruptMode::Change);
        TEST_ASSERT_EQUAL(ttime::mono::now().micros(), i.wakeAt().micros());
    }
}

int called = 0;

TEST_F(t_interrupt, test_calls_isr) {
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
