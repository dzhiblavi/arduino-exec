#include "Executor.h"

#include <exec/os/service/TimerService.h>

#include <time/config.h>
#include <utest/utest.h>

test::Executor ex;

void setUp() {
    static_assert(TIME_MANUAL, "manual time is expected for tests");
    ttime::mono::set(ttime::Time());
    exec::setExecutor(&ex);
    ex.queued.clear();
}

namespace exec {

auto noopRunnable = runnable([](auto) {});

TEST(test_timer_not_ready) {
    HeapTimerService<2> s;
    TimerEntry t;
    t.at = ttime::Time(10);
    t.task = &noopRunnable;

    s.add(&t);
    ttime::mono::advance(ttime::Duration(5));

    s.tick();
    TEST_ASSERT_EQUAL(0, ex.queued.size());
}

TEST(test_timer_ready) {
    HeapTimerService<2> s;
    TimerEntry t;
    t.at = ttime::Time(10);
    t.task = &noopRunnable;

    s.add(&t);
    ttime::mono::advance(ttime::Duration(15));

    s.tick();
    TEST_ASSERT_EQUAL(1, ex.queued.size());
}

TEST(test_timer_cancel_ok) {
    HeapTimerService<2> s;
    TimerEntry t;
    t.at = ttime::Time(10);
    t.task = &noopRunnable;

    s.add(&t);
    s.tick();

    TEST_ASSERT_TRUE(s.remove(&t));
    ttime::mono::advance(ttime::Duration(15));

    s.tick();
    TEST_ASSERT_EQUAL(0, ex.queued.size());
}

TEST(test_timer_cancel_gone) {
    HeapTimerService<2> s;
    TimerEntry t;
    t.at = ttime::Time(10);
    t.task = &noopRunnable;

    s.add(&t);
    ttime::mono::advance(ttime::Duration(15));
    s.tick();

    TEST_ASSERT_FALSE(s.remove(&t));
    TEST_ASSERT_EQUAL(1, ex.queued.size());
}

}  // namespace exec

TESTS_MAIN
