#include <exec/os/CronService.h>

#include <time/config.h>
#include <utest/utest.h>

void setUp() {
    static_assert(TIME_MANUAL, "manual time is expected for tests");
    ttime::mono::set(ttime::Time());
}

namespace exec {

template <typename F>
struct Task : CronTask {
    Task(F task, ttime::Duration d, ttime::Time at = ttime::mono::now())
        : CronTask(d, at)
        , task{task} {}

    void run() override { task(this); }

    F task;
};

auto inc(int& c) {
    return [&](auto) { ++c; };
}

TEST(test_cron_tick_no_tasks) {
    HeapCronService<2> s;
    s.tick();
}

TEST(test_cron_at) {
    HeapCronService<2> s;

    int c1 = 0;
    auto r1 = Task(inc(c1), ttime::Duration(10), ttime::Time(10));
    s.add(&r1);

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
    auto r1 = Task(inc(c1), ttime::Duration(10));
    s.add(&r1);

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
    auto r1 = Task(inc(c), ttime::Duration(30));
    auto r2 = Task(inc(c), ttime::Duration(10));
    auto r3 = Task(inc(c), ttime::Duration(20));

    SECTION("no tasks") {
        TEST_ASSERT_EQUAL(ttime::Time::max().micros(), s.wakeAt().micros());
    }

    SECTION("task ready") {
        s.add(&r1);
        TEST_ASSERT_EQUAL(ttime::mono::now().micros(), s.wakeAt().micros());
    }

    SECTION("multiple tasks") {
        s.add(&r1);
        s.add(&r2);
        s.add(&r3);
        s.tick();
        TEST_ASSERT_EQUAL(10, s.wakeAt().micros());
    }
}

TEST(test_cron_remove) {
    HeapCronService<3> s;
    int c = 0;
    auto r1 = Task(inc(c), ttime::Duration(30));
    auto r2 = Task(inc(c), ttime::Duration(10));
    auto r3 = Task(inc(c), ttime::Duration(20));

    SECTION("not linked") {
        TEST_ASSERT_FALSE(s.remove(&r1));
    }

    SECTION("task linked") {
        s.add(&r1);
        TEST_ASSERT_TRUE(s.remove(&r1));
        TEST_ASSERT_FALSE(r1.connected());
    }

    SECTION("remove => task is not executed") {
        s.add(&r1);
        s.remove(&r1);
        s.tick();
        TEST_ASSERT_EQUAL(0, c);
        TEST_ASSERT_EQUAL(ttime::Time::max().micros(), s.wakeAt().micros());
    }
}

TEST(test_cron_remove_self_in_callback) {
    HeapCronService<3> s;

    int cnt1 = 0, cnt2 = 0;
    bool r1 = false, r2 = false;

    auto task = [&s](bool& rem, int& cnt) {
        return [&](auto self) {
            ++cnt;
            if (rem) {
                s.remove(self);
            }
        };
    };

    auto c1 = Task(task(r1, cnt1), ttime::Duration(10));
    auto c2 = Task(task(r2, cnt2), ttime::Duration(30));

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

    CronTask *e1 = nullptr, *e2 = nullptr;

    auto task = [&s](bool& rem, int& cnt, CronTask*& task) {
        return [&](auto) {
            ++cnt;
            if (rem) {
                s.remove(task);
            }
        };
    };

    auto c1 = Task(task(r1, cnt1, e2), ttime::Duration(10));
    auto c2 = Task(task(r2, cnt2, e1), ttime::Duration(30));
    e1 = &c1;
    e2 = &c2;

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

    auto task = [&s](int& cnt) {
        return [&](auto self) {
            ++cnt;
            s.remove(self);
        };
    };

    int c = 0;
    auto t = Task(task(c), ttime::Duration(30));

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
