#include <exec/Runnable.h>
#include <exec/os/OS.h>

#include <time/config.h>
#include <utest/utest.h>

namespace exec {

struct MockService : Service {
    void tick() override {
        ++cnt;
    }

    ttime::Time wakeAt() const override {
        return wake_at;
    }

    int cnt = 0;
    ttime::Time wake_at = ttime::Time::max();
};

auto makeTask(int& cnt) {
    return runnable([&cnt](auto) { ++cnt; });
}

TEST(test_tick) {
    OS os;

    SECTION("no services") {
        os.tick();
    }

    SECTION("has services") {
        MockService a, b;
        os.addService(&a);
        os.addService(&b);
        os.tick();

        TEST_ASSERT_EQUAL(1, a.cnt);
        TEST_ASSERT_EQUAL(1, b.cnt);
    }
}

TEST(test_wake_at) {
    OS os;

    SECTION("no services") {
        TEST_ASSERT_EQUAL(ttime::Time::max().millis(), os.wakeAt().millis());
    }

    SECTION("has services") {
        MockService a, b, c;
        a.wake_at = ttime::Time(10);
        b.wake_at = ttime::Time::max();
        c.wake_at = ttime::Time(20);
        os.addService(&a);
        os.addService(&b);
        os.addService(&c);

        TEST_ASSERT_EQUAL(ttime::Time(10).millis(), os.wakeAt().millis());
    }
}

}  // namespace exec

TESTS_MAIN
