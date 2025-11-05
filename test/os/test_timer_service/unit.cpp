#include <exec/os/TimerService.h>

#include <time/config.h>
#include <utest/utest.h>

void setUp() {
    static_assert(TIME_MANUAL, "manual time is expected for tests");
    ttime::mono::set(ttime::Time());
}

namespace exec {

auto makeTask(int& cnt) {
    struct Task : TimerEntry {
        Task(int& cnt) : cnt{cnt} {}

        Runnable* run() override {
            ++cnt;
            return noop;
        }

        int& cnt;
    };

    return Task{cnt};
}

TEST(test_timer_not_ready) {
    int cnt = 0;
    auto t = makeTask(cnt);
    HeapTimerService<2> s;

    t.at = ttime::Time(10);

    s.add(&t);
    ttime::mono::advance(ttime::Duration(5));

    s.tick();
    TEST_ASSERT_EQUAL(0, cnt);
}

TEST(test_timer_ready) {
    int cnt = 0;
    auto t = makeTask(cnt);
    HeapTimerService<2> s;

    t.at = ttime::Time(10);

    s.add(&t);
    ttime::mono::advance(ttime::Duration(15));

    s.tick();
    TEST_ASSERT_EQUAL(1, cnt);
}

TEST(test_timer_cancel_ok) {
    int cnt = 0;
    auto t = makeTask(cnt);
    HeapTimerService<2> s;

    t.at = ttime::Time(10);

    s.add(&t);
    s.tick();

    TEST_ASSERT_TRUE(s.remove(&t));
    ttime::mono::advance(ttime::Duration(15));

    s.tick();
    TEST_ASSERT_EQUAL(0, cnt);
}

TEST(test_timer_cancel_gone) {
    int cnt = 0;
    auto t = makeTask(cnt);
    HeapTimerService<2> s;

    t.at = ttime::Time(10);

    s.add(&t);
    ttime::mono::advance(ttime::Duration(15));
    s.tick();

    TEST_ASSERT_FALSE(s.remove(&t));
    TEST_ASSERT_EQUAL(1, cnt);
}

TEST(test_wake_at) {
    HeapTimerService<2> s;

    SECTION("no defers") {
        TEST_ASSERT_EQUAL(ttime::Time::max().micros(), s.wakeAt().micros());
    }

    SECTION("with defers") {
        int cnt = 0;
        auto t = makeTask(cnt);
        t.at = ttime::Time(10);

        s.add(&t);
        TEST_ASSERT_EQUAL(ttime::Time(10).micros(), s.wakeAt().micros());
    }
}

}  // namespace exec

TESTS_MAIN
