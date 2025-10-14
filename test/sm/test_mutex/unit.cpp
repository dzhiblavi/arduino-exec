#include "Executor.h"

#include <exec/executor/Executor.h>
#include <exec/sm/sync/Mutex.h>

#include <utest/utest.h>

namespace exec {

struct t_mutex {
    struct Task : Runnable {
        Task() = default;

        Runnable* run() override {
            return noop;
        }
    };

    t_mutex() {
        exec::setExecutor(&executor);
    }

    Task t1;
    Task t2;
    Mutex m;
    test::Executor executor;
};

TEST_F(t_mutex, test_try_lock) {
    TEST_ASSERT_TRUE(m.tryLock());
    TEST_ASSERT_FALSE(m.tryLock());
    m.unlock();
    TEST_ASSERT_TRUE(m.tryLock());
    TEST_ASSERT_TRUE(executor.queued.empty());
}

TEST_F(t_mutex, test_lock_unlocked) {
    TEST_ASSERT_EQUAL(&t1, m.lock()(&t1));
    TEST_ASSERT_TRUE(executor.queued.empty());
}

TEST_F(t_mutex, test_lock_locked) {
    m.lock()(&t1);
    TEST_ASSERT_EQUAL(noop, m.lock()(&t2));
    TEST_ASSERT_TRUE(executor.queued.empty());
    m.unlock();
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    TEST_ASSERT_EQUAL(&t2, executor.queued.front());
}

}  // namespace exec

TESTS_MAIN
