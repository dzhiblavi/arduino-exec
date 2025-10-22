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

TEST(test_cron_remove_self_in_callback) {
    HeapCronService<3> s;

    int cnt1 = 0, cnt2 = 0;
    bool r1 = false, r2 = false;
    auto c1 = CronTask(ttime::Duration(10));
    auto c2 = CronTask(ttime::Duration(30));

    auto task = [&s](bool& rem, auto& task, int& cnt) {
        return runnable([&](auto) {
            ++cnt;
            if (rem) {
                s.remove(&task);
            }
        });
    };

    auto t1 = task(r1, c1, cnt1);
    auto t2 = task(r2, c2, cnt2);

    c1.task = &t1;
    c2.task = &t2;

    s.add(&c1);
    s.add(&c2);

    SECTION("remove front") {
        r1 = true;
        s.tick();
        TEST_ASSERT_FALSE(c1.connected());
        TEST_ASSERT_TRUE(c2.connected());
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(1, cnt2);

        ttime::mono::advance(ttime::Duration(10));
        s.tick();
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(1, cnt2);

        ttime::mono::advance(ttime::Duration(20));
        s.tick();
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(2, cnt2);
    }

    SECTION("remove back") {
        r2 = true;
        s.tick();
        TEST_ASSERT_TRUE(c1.connected());
        TEST_ASSERT_FALSE(c2.connected());
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(1, cnt2);

        ttime::mono::advance(ttime::Duration(5));
        s.tick();
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(1, cnt2);

        ttime::mono::advance(ttime::Duration(5));
        s.tick();
        TEST_ASSERT_EQUAL(2, cnt1);
        TEST_ASSERT_EQUAL(1, cnt2);
    }
}

TEST(test_cron_remove_other_in_callback) {
    HeapCronService<3> s;

    int cnt1 = 0, cnt2 = 0;
    bool r1 = false, r2 = false;
    auto c1 = CronTask(ttime::Duration(10));
    auto c2 = CronTask(ttime::Duration(30));

    auto task = [&s](bool& rem, auto& task, int& cnt) {
        return runnable([&](auto) {
            ++cnt;
            if (rem) {
                s.remove(&task);
            }
        });
    };

    auto t1 = task(r1, c2, cnt1);
    auto t2 = task(r2, c1, cnt2);

    c1.task = &t1;
    c2.task = &t2;

    s.add(&c1);
    s.add(&c2);

    SECTION("remove front") {
        r2 = true;  // removes c1 after it has executed
        s.tick();
        TEST_ASSERT_FALSE(c1.connected());
        TEST_ASSERT_TRUE(c2.connected());
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(1, cnt2);

        ttime::mono::advance(ttime::Duration(10));
        s.tick();
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(1, cnt2);

        ttime::mono::advance(ttime::Duration(20));
        s.tick();
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(2, cnt2);
    }

    SECTION("remove back") {
        r1 = true;  // removes c2 before it has executed
        s.tick();
        TEST_ASSERT_TRUE(c1.connected());
        TEST_ASSERT_FALSE(c2.connected());
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(0, cnt2);

        ttime::mono::advance(ttime::Duration(5));
        s.tick();
        TEST_ASSERT_EQUAL(1, cnt1);
        TEST_ASSERT_EQUAL(0, cnt2);

        ttime::mono::advance(ttime::Duration(5));
        s.tick();
        TEST_ASSERT_EQUAL(2, cnt1);
        TEST_ASSERT_EQUAL(0, cnt2);
    }
}

TEST(test_cron_remove_then_add_back) {
    HeapCronService<1> s;

    auto task = [&s](auto& task, int& cnt) {
        return runnable([&](auto) {
            ++cnt;
            s.remove(&task);
        });
    };

    int c = 0;
    auto t = CronTask(ttime::Duration(30));
    auto r = task(t, c);
    t.task = &r;

    // task removes itself
    s.add(&t);
    s.tick();
    TEST_ASSERT_FALSE(t.connected());
    TEST_ASSERT_EQUAL(1, c);

    // it anyways should be scheduled correctly
    TEST_ASSERT_EQUAL(30, t.at.micros());

    // but should not be executed while removed
    s.tick();
    TEST_ASSERT_EQUAL(1, c);

    // should be executed @30 when reinserted
    s.add(&t);
    s.tick();
    TEST_ASSERT_EQUAL(1, c);

    ttime::mono::advance(ttime::Duration(30));
    s.tick();
    TEST_ASSERT_EQUAL(2, c);
}

}  // namespace exec

TESTS_MAIN
