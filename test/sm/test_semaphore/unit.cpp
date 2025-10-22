#include "Executor.h"

#include <exec/os/Service.h>
#include <exec/executor/Executor.h>
#include <exec/sm/sync/Semaphore.h>

#include <utest/utest.h>

namespace exec {

struct t_semaphore {
    struct Task : Runnable {
        Task() = default;

        Runnable* run() override {
            return noop;
        }
    };

    t_semaphore() {
        exec::setService<exec::Executor>(&executor);
    }

    Task t1;
    Task t2;
    Task t3;
    Semaphore s{2};
    test::Executor executor;
};

TEST_F(t_semaphore, test_try_acquire) {
    TEST_ASSERT_TRUE(s.tryAcquire());
    TEST_ASSERT_TRUE(s.tryAcquire());
    TEST_ASSERT_FALSE(s.tryAcquire());
    s.release();
    TEST_ASSERT_TRUE(s.tryAcquire());
    TEST_ASSERT_FALSE(s.tryAcquire());
    TEST_ASSERT_TRUE(executor.queued.empty());
}

TEST_F(t_semaphore, test_acquire_available) {
    TEST_ASSERT_EQUAL(&t1, s.acquire()(&t1));
    TEST_ASSERT_EQUAL(&t2, s.acquire()(&t2));
    s.release();
    TEST_ASSERT_EQUAL(&t3, s.acquire()(&t3));
    TEST_ASSERT_TRUE(executor.queued.empty());
}

TEST_F(t_semaphore, test_acquire_unavailable) {
    s.tryAcquire();
    s.tryAcquire();
    TEST_ASSERT_EQUAL(noop, s.acquire()(&t1));
    TEST_ASSERT_TRUE(executor.queued.empty());
    s.release();
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(&t1, executor.queued.front());
}

}  // namespace exec

TESTS_MAIN
