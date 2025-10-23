#include "sm/channels/mpmc.h"

#include <exec/sm/sync/MPMCChannel.h>

#include <utest/utest.h>

namespace exec {

using Op = ChanOp<int>;

struct t_mpmc_channel : test::t_mpmc_channel<Op, Op, MPMCChannel<int, 2>> {};

TEST_F(t_mpmc_channel, test_send_nonblocking) {
    test_send_nonblocking();
}

TEST_F(t_mpmc_channel, test_receive_nonblocking) {
    test_receive_nonblocking();
}

TEST_F(t_mpmc_channel, test_send_blocking) {
    test_send_blocking();
}

TEST_F(t_mpmc_channel, test_send_blocking_connects_cancellation) {
    test_send_blocking_connects_cancellation();
}

TEST_F(t_mpmc_channel, test_receive_blocking_connects_cancellation) {
    test_receive_blocking_connects_cancellation();
}

TEST_F(t_mpmc_channel, test_send_blocking_cancelled) {
    test_send_blocking_cancelled();
}

TEST_F(t_mpmc_channel, test_receive_blocking_cancelled) {
    test_receive_blocking_cancelled();
}

TEST_F(t_mpmc_channel, test_send_blocking_rendezvouz) {
    test_send_blocking_rendezvouz();
}

TEST_F(t_mpmc_channel, test_receive_blocking_rendezvouz) {
    test_receive_blocking_rendezvouz();
}

TEST_F(t_mpmc_channel, test_send_blocks_multiple_producers) {
    test_send_blocks_multiple_producers();
}

TEST_F(t_mpmc_channel, test_send_blocks_multiple_producers_connects_cancellation) {
    test_send_blocks_multiple_producers_connects_cancellation();
}

TEST_F(t_mpmc_channel, test_send_blocks_multiple_producers_both_cancelled) {
    test_send_blocks_multiple_producers_both_cancelled();
}

TEST_F(t_mpmc_channel, test_send_blocks_multiple_producers_one_cancelled) {
    test_send_blocks_multiple_producers_one_cancelled();
}

TEST_F(t_mpmc_channel, test_send_blocks_multiple_producers_both_released) {
    test_send_blocks_multiple_producers_both_released();
}

TEST_F(t_mpmc_channel, test_receive_blocks_multiple_consumers) {
    test_receive_blocks_multiple_consumers();
}

TEST_F(t_mpmc_channel, test_receive_blocks_multiple_consumers_connects_cancellation) {
    test_receive_blocks_multiple_consumers_connects_cancellation();
}

TEST_F(t_mpmc_channel, test_receive_blocks_multiple_consumers_both_cancelled) {
    test_receive_blocks_multiple_consumers_both_cancelled();
}

TEST_F(t_mpmc_channel, test_receive_blocks_multiple_consumers_one_cancelled) {
    test_receive_blocks_multiple_consumers_one_cancelled();
}

TEST_F(t_mpmc_channel, test_receive_blocks_multiple_consumers_both_released) {
    test_receive_blocks_multiple_consumers_both_released();
}

}  // namespace exec

TESTS_MAIN
