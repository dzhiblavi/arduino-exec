#include "sm/channels/mpsc.h"

#include <exec/sm/sync/MPSCChannel.h>

#include <utest/utest.h>

namespace exec {

using SendOp = ChanSendOp<int>;

struct RecvOp {
    Initiator auto receive(MPSCChannel<int, 2>* c, int* dst, ErrCode* ec) {
        return [c, dst, ec](Runnable* cb, CancellationSlot slot = {}) {
            return c->receive(dst, ec)(cb, slot);
        };
    }
};

struct t_mpsc_channel : test::t_mpsc_channel<SendOp, RecvOp, MPSCChannel<int, 2>> {};

TEST_F(t_mpsc_channel, test_send_nonblocking) {
    test_send_nonblocking();
}

TEST_F(t_mpsc_channel, test_receive_nonblocking) {
    test_receive_nonblocking();
}

TEST_F(t_mpsc_channel, test_send_blocking) {
    test_send_blocking();
}

TEST_F(t_mpsc_channel, test_send_blocking_connects_cancellation) {
    test_send_blocking_connects_cancellation();
}

TEST_F(t_mpsc_channel, test_receive_blocking_connects_cancellation) {
    test_receive_blocking_connects_cancellation();
}

TEST_F(t_mpsc_channel, test_send_blocking_cancelled) {
    test_send_blocking_cancelled();
}

TEST_F(t_mpsc_channel, test_receive_blocking_cancelled) {
    test_receive_blocking_cancelled();
}

TEST_F(t_mpsc_channel, test_send_blocking_rendezvouz) {
    test_send_blocking_rendezvouz();
}

TEST_F(t_mpsc_channel, test_receive_blocking_rendezvouz) {
    test_receive_blocking_rendezvouz();
}

TEST_F(t_mpsc_channel, test_send_blocks_multiple_producers) {
    test_send_blocks_multiple_producers();
}

TEST_F(t_mpsc_channel, test_send_blocks_multiple_producers_connects_cancellation) {
    test_send_blocks_multiple_producers_connects_cancellation();
}

TEST_F(t_mpsc_channel, test_send_blocks_multiple_producers_both_cancelled) {
    test_send_blocks_multiple_producers_both_cancelled();
}

TEST_F(t_mpsc_channel, test_send_blocks_multiple_producers_one_cancelled) {
    test_send_blocks_multiple_producers_one_cancelled();
}

TEST_F(t_mpsc_channel, test_send_blocks_multiple_producers_both_released) {
    test_send_blocks_multiple_producers_both_released();
}

}  // namespace exec

TESTS_MAIN
