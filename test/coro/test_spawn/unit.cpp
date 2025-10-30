#include "Executor.h"

#include <exec/coro/spawn.h>
#include <exec/coro/sync/Event.h>

#include <utest/utest.h>

namespace exec {

struct t_spawn {
    t_spawn() {
        setService<Executor>(&executor);
    }

    test::Executor executor;
};

TEST_F(t_spawn, simple_spawn) {
    bool done = false;
    Event event;

    auto coro = [&]() -> Async<> {
        co_await event.wait();
        done = true;
    };

    spawn(coro());
    TEST_ASSERT_EQUAL(1, executor.queued.size());
    executor.queued.popFront()->runAll();
    TEST_ASSERT_FALSE(done);

    event.fireOnce();
    TEST_ASSERT_TRUE(done);
}

}  // namespace exec

TESTS_MAIN
