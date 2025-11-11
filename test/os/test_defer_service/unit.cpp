#include <exec/os/DeferService.h>

#include <time/config.h>
#include <utest/utest.h>

void setUp() {
    static_assert(TIME_MANUAL, "manual time is expected for tests");
    ttime::mono::set(ttime::Time());
}

namespace exec {

auto makeTask(int& cnt) {
    return runnable([&cnt](auto) { ++cnt; });
}

TEST(test_tick_empty) {
    HeapDeferService<2> ds;

    ds.tick();
}

TEST(test_defer_not_ready) {
    int cnt = 0;
    auto task = makeTask(cnt);
    HeapDeferService<2> ds;

    ds.defer(&task, ttime::Time(10));
    ttime::mono::advance(ttime::Duration(5));

    ds.tick();
    TEST_ASSERT_EQUAL(0, cnt);
}

TEST(test_defer_ready) {
    int cnt = 0;
    auto task = makeTask(cnt);
    HeapDeferService<2> ds;

    ds.defer(&task, ttime::Time(10));
    ttime::mono::advance(ttime::Duration(15));

    ds.tick();
    TEST_ASSERT_EQUAL(1, cnt);
}

TEST(test_wake_at) {
    HeapDeferService<2> ds;

    SECTION("no defers") {
        TEST_ASSERT_EQUAL(ttime::Time::max().millis(), ds.wakeAt().millis());
    }

    SECTION("with defers") {
        int cnt = 0;
        auto task = makeTask(cnt);

        ds.defer(&task, ttime::Time(10));
        TEST_ASSERT_EQUAL(ttime::Time(10).millis(), ds.wakeAt().millis());
    }
}

}  // namespace exec

TESTS_MAIN
