#include <exec/executor/SystemExecutor.h>

#include <utest/utest.h>

namespace exec {

TEST(test_run_empty) {
    SystemExecutor exec;
    exec.tick();
}

TEST(test_run_order) {
    SystemExecutor exec;
    int counter = 0;

    auto task = [&counter](int expected) {
        return runnable([expected, &counter](auto) {
            TEST_ASSERT_EQUAL(expected, counter);
            ++counter;
        });
    };

    auto t0 = task(0);
    auto t1 = task(1);
    auto t2 = task(2);

    exec.post(&t0);
    exec.post(&t1);
    exec.post(&t2);

    exec.tick();
    TEST_ASSERT_EQUAL(3, counter);
}

TEST(test_run_order_push_during_tick) {
    SystemExecutor exec;
    int counter = 0;

    auto task = [&](int expected, Runnable* post) {
        return runnable([&, expected, post](auto) {
            TEST_ASSERT_EQUAL(expected, counter);
            if (post != nullptr) {
                exec.post(post);
            }
            ++counter;
        });
    };

    auto t3 = task(3, nullptr);
    auto t4 = task(4, nullptr);
    auto t5 = task(5, nullptr);
    auto t0 = task(0, &t3);
    auto t1 = task(1, &t4);
    auto t2 = task(2, &t5);

    exec.post(&t0);
    exec.post(&t1);
    exec.post(&t2);

    exec.tick();
    TEST_ASSERT_EQUAL(3, counter);

    exec.tick();
    TEST_ASSERT_EQUAL(6, counter);
}

TEST(test_wake_at) {
    SystemExecutor exec;

    SECTION("empty queue") {
        TEST_ASSERT_EQUAL(ttime::Time::max().millis(), exec.wakeAt().millis());
    }

    SECTION("non-empty queue") {
        auto task = runnable([](auto) {});
        exec.post(&task);
        TEST_ASSERT_EQUAL(ttime::mono::now().millis(), exec.wakeAt().millis());
    }
}

}  // namespace exec

TESTS_MAIN
