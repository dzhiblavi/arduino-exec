#include "sm/channels/spsc.h"

#include <exec/sm/sync/SPSCChannel.h>

#include <utest/utest.h>

namespace exec {

struct Op {
    Initiator auto send(SPSCChannel<int, 2>* c, int value, ErrCode* ec) {
        return [c, value, ec](Runnable* cb, CancellationSlot slot = {}) {
            return c->send(value, ec)(cb, slot);
        };
    }

    Initiator auto receive(SPSCChannel<int, 2>* c, int* dst, ErrCode* ec) {
        return [c, dst, ec](Runnable* cb, CancellationSlot slot = {}) {
            return c->receive(dst, ec)(cb, slot);
        };
    }
};

struct t_spsc_channel : test::t_spsc_channel<Op, Op, SPSCChannel<int, 2>> {};

TEST_F(t_spsc_channel, test_send_nonblocking) {
    test_send_nonblocking();
}

TEST_F(t_spsc_channel, test_receive_nonblocking) {
    test_receive_nonblocking();
}

TEST_F(t_spsc_channel, test_send_blocking) {
    test_send_blocking();
}

TEST_F(t_spsc_channel, test_send_blocking_connects_cancellation) {
    test_send_blocking_connects_cancellation();
}

TEST_F(t_spsc_channel, test_receive_blocking_connects_cancellation) {
    test_receive_blocking_connects_cancellation();
}

TEST_F(t_spsc_channel, test_send_blocking_cancelled) {
    test_send_blocking_cancelled();
}

TEST_F(t_spsc_channel, test_receive_blocking_cancelled) {
    test_receive_blocking_cancelled();
}

TEST_F(t_spsc_channel, test_send_blocking_rendezvouz) {
    test_send_blocking_rendezvouz();
}

TEST_F(t_spsc_channel, test_receive_blocking_rendezvouz) {
    test_receive_blocking_rendezvouz();
}

}  // namespace exec

TESTS_MAIN
