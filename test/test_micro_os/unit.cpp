#include "Executor.h"

#include <exec/os/MicroOS.h>

#include <utest/utest.h>
#include <time/config.h>

void setUp() {
    static_assert(TIME_MANUAL, "manual time is expected for tests");
    ttime::mono::set(ttime::Time());
}

namespace exec {

auto noopRunnable = runnable([](auto) {});

TEST(test_tick_empty) {
    test::Executor exec;
    MicroOS<2, 2> os(&exec);

    os.tick();
    TEST_ASSERT_EQUAL(0, exec.queued.size());
}

TEST(test_defer_not_ready) {
    test::Executor exec;
    MicroOS<2, 2> os(&exec);

    os.defer(&noopRunnable, ttime::Time(10));
    ttime::mono::advance(ttime::Duration(5));

    os.tick();
    TEST_ASSERT_EQUAL(0, exec.queued.size());
}

TEST(test_defer_ready) {
    test::Executor exec;
    MicroOS<2, 2> os(&exec);

    os.defer(&noopRunnable, ttime::Time(10));
    ttime::mono::advance(ttime::Duration(15));

    os.tick();
    TEST_ASSERT_EQUAL(1, exec.queued.size());
    TEST_ASSERT_EQUAL(&noopRunnable, exec.queued.front());
}

TEST(test_timer_not_ready) {
    test::Executor exec;
    MicroOS<2, 2> os(&exec);

    TimerEntry t;
    t.at = ttime::Time(10);
    t.task = &noopRunnable;

    os.add(&t);
    ttime::mono::advance(ttime::Duration(5));

    os.tick();
    TEST_ASSERT_EQUAL(0, exec.queued.size());
}

TEST(test_timer_ready) {
    test::Executor exec;
    MicroOS<2, 2> os(&exec);

    TimerEntry t;
    t.at = ttime::Time(10);
    t.task = &noopRunnable;

    os.add(&t);
    ttime::mono::advance(ttime::Duration(15));

    os.tick();
    TEST_ASSERT_EQUAL(1, exec.queued.size());
}

TEST(test_timer_cancel_ok) {
    test::Executor exec;
    MicroOS<2, 2> os(&exec);

    TimerEntry t;
    t.at = ttime::Time(10);
    t.task = &noopRunnable;

    os.add(&t);
    os.tick();

    TEST_ASSERT_TRUE(os.remove(&t));
    ttime::mono::advance(ttime::Duration(15));

    os.tick();
    TEST_ASSERT_EQUAL(0, exec.queued.size());
}

TEST(test_timer_cancel_gone) {
    test::Executor exec;
    MicroOS<2, 2> os(&exec);

    TimerEntry t;
    t.at = ttime::Time(10);
    t.task = &noopRunnable;

    os.add(&t);
    ttime::mono::advance(ttime::Duration(15));
    os.tick();

    TEST_ASSERT_FALSE(os.remove(&t));
    TEST_ASSERT_EQUAL(1, exec.queued.size());
}

}  // namespace exec

TESTS_MAIN
