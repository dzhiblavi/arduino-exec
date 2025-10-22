#include <exec/os/CronService.h>

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

TEST(test_cron_tick_no_tasks) {
    HeapCronService<2> s;
    s.tick();
}

TEST(test_cron_at) {
    HeapCronService<2> s;

    int c1 = 0;
    auto r1 = makeTask(c1);
    auto t1 = CronTask(&r1, ttime::Duration(10), ttime::Time(10));
    s.add(&t1);

    SECTION("not ready") {
        s.tick();
        TEST_ASSERT_EQUAL(0, c1);
    }

    SECTION("ready") {
        ttime::mono::set(ttime::Time(15));
        s.tick();
        TEST_ASSERT_EQUAL(1, c1);
    }
}

TEST(test_cron_interval) {
    HeapCronService<2> s;

    int c1 = 0;
    auto r1 = makeTask(c1);
    auto t1 = CronTask(&r1, ttime::Duration(10));
    s.add(&t1);

    s.tick();
    TEST_ASSERT_EQUAL(1, c1);

    ttime::mono::advance(ttime::Duration(5));

    s.tick();
    TEST_ASSERT_EQUAL(1, c1);

    ttime::mono::advance(ttime::Duration(5));

    s.tick();
    TEST_ASSERT_EQUAL(2, c1);

    ttime::mono::advance(ttime::Duration(5));

    s.tick();
    TEST_ASSERT_EQUAL(2, c1);
}

TEST(test_cron_wake_at) {
    HeapCronService<3> s;
    int c = 0;
    auto r1 = makeTask(c);
    auto r2 = makeTask(c);
    auto r3 = makeTask(c);
    auto t1 = CronTask(&r1, ttime::Duration(30));
    auto t2 = CronTask(&r2, ttime::Duration(10));
    auto t3 = CronTask(&r3, ttime::Duration(20));

    SECTION("no tasks") {
        TEST_ASSERT_EQUAL(ttime::Time::max().micros(), s.wakeAt().micros());
    }

    SECTION("task ready") {
        s.add(&t1);
        TEST_ASSERT_EQUAL(ttime::mono::now().micros(), s.wakeAt().micros());
    }

    SECTION("multiple tasks") {
        s.add(&t1);
        s.add(&t2);
        s.add(&t3);
        s.tick();
        TEST_ASSERT_EQUAL(10, s.wakeAt().micros());
    }
}

TEST(test_cron_remove) {
    HeapCronService<3> s;
    int c = 0;
    auto r1 = makeTask(c);
    auto r2 = makeTask(c);
    auto r3 = makeTask(c);
    auto t1 = CronTask(&r1, ttime::Duration(30));
    auto t2 = CronTask(&r2, ttime::Duration(10));
    auto t3 = CronTask(&r3, ttime::Duration(20));

    SECTION("not linked") {
        TEST_ASSERT_FALSE(s.remove(&t1));
    }

    SECTION("task linked") {
        s.add(&t1);
        TEST_ASSERT_TRUE(s.remove(&t1));
        TEST_ASSERT_FALSE(t1.connected());
    }

    SECTION("remove => task is not executed") {
        s.add(&t1);
        s.remove(&t1);
        s.tick();
        TEST_ASSERT_EQUAL(0, c);
        TEST_ASSERT_EQUAL(ttime::Time::max().micros(), s.wakeAt().micros());
    }
}

}  // namespace exec

TESTS_MAIN
