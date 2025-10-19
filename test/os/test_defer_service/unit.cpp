#include "Executor.h"

#include <exec/os//DeferService.h>

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

TEST(test_tick_empty) {
    HeapDeferService<2> ds;

    ds.tick();
    TEST_ASSERT_EQUAL(0, ex.queued.size());
}

TEST(test_defer_not_ready) {
    HeapDeferService<2> ds;

    ds.defer(&noopRunnable, ttime::Time(10));
    ttime::mono::advance(ttime::Duration(5));

    ds.tick();
    TEST_ASSERT_EQUAL(0, ex.queued.size());
}

TEST(test_defer_ready) {
    HeapDeferService<2> ds;

    ds.defer(&noopRunnable, ttime::Time(10));
    ttime::mono::advance(ttime::Duration(15));

    ds.tick();
    TEST_ASSERT_EQUAL(1, ex.queued.size());
    TEST_ASSERT_EQUAL(&noopRunnable, ex.queued.front());
}

}  // namespace exec

TESTS_MAIN
