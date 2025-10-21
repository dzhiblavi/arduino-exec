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

}  // namespace exec

TESTS_MAIN
