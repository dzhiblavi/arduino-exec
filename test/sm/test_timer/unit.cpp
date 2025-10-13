#include "OS.h"

#include <exec/os/global.h>
#include <exec/sm/Timer.h>

#include <utest/utest.h>

test::OS test_os;

void globalSetUp() {
    exec::setOS(&test_os);
}

void setUp() {
    static_assert(TIME_MANUAL, "manual time is expected for tests");
    ttime::mono::set(ttime::Time());
}

namespace exec {

auto noopRunnable = runnable([](auto) {});

TEST(test_timer_enqueues) {
    auto cb = runnable([&](auto) {});
    ErrCode ec = ErrCode::Unknown;
    Timer t;

    test_os.next_add = &t;
    TEST_ASSERT_TRUE(noop == t.wait(ttime::Duration(), &ec)(&cb));
}

TEST(test_timer_installs_cancellation) {
    auto cb = runnable([&](auto) {});
    ErrCode ec = ErrCode::Unknown;
    Timer t;
    CancellationSignal sig;

    test_os.next_add = &t;
    t.wait(ttime::Duration(), &ec)(&cb, sig.slot());
    TEST_ASSERT_TRUE(sig.hasHandler());
}

TEST(test_timer_goes_off) {
    auto cb = runnable([&](auto) {});
    ErrCode ec = ErrCode::Unknown;
    Timer t;
    CancellationSignal sig;

    test_os.next_add = &t;
    t.wait(ttime::Duration(), &ec)(&cb, sig.slot());

    TEST_ASSERT_EQUAL(&cb, test_os.last_added->run());
    TEST_ASSERT_FALSE(sig.hasHandler());
}

TEST(test_timer_cancel_success) {
    auto cb = runnable([&](auto) {});
    ErrCode ec = ErrCode::Unknown;
    Timer t;
    CancellationSignal sig;

    test_os.next_add = &t;
    t.wait(ttime::Duration(), &ec)(&cb, sig.slot());

    test_os.next_remove = &t;
    test_os.remove_result = true;
    TEST_ASSERT_EQUAL(&cb, sig.emitRaw());
    TEST_ASSERT_FALSE(sig.hasHandler());
    TEST_ASSERT_EQUAL(ErrCode::Cancelled, ec);
}

TEST(test_timer_cancel_too_late) {
    bool called = false;
    auto cb = runnable([&](auto) { called = true; });
    ErrCode ec = ErrCode::Unknown;
    Timer t;
    CancellationSignal sig;

    test_os.next_add = &t;
    t.wait(ttime::Duration(), &ec)(&cb, sig.slot());

    test_os.next_remove = &t;
    test_os.remove_result = false;

    TEST_ASSERT_EQUAL(noop, sig.emitRaw());
    TEST_ASSERT_FALSE(sig.hasHandler());
    TEST_ASSERT_EQUAL(&cb, test_os.last_added->run());
    TEST_ASSERT_EQUAL(ErrCode::Success, ec);
}

}  // namespace exec

TESTS_MAIN
