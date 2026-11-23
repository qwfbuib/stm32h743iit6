
/* SPDX-License-Identifier: Apache-2.0 */
#include "can_diag.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

struct fake_can {
    enum can_mode mode;
    enum can_result tx_result;
    enum can_result rx_result;
    uint32_t now;
    unsigned int normal_starts;
    unsigned int loopback_starts;
    unsigned int aborts;
    unsigned int queued;
    unsigned int fail_normal_on;
    bool fail_loopback;
    bool corrupt_frame;
    bool reject_queue;
    struct can_frame frame;
};

static bool fake_start(void *context, enum can_mode mode)
{
    struct fake_can *fake = context;
    fake->mode = CAN_DOWN;
    if (mode == CAN_NORMAL) {
        if (++fake->normal_starts == fake->fail_normal_on) {
            return false;
        }
    } else {
        ++fake->loopback_starts;
        if (fake->fail_loopback) {
            return false;
        }
    }
    fake->mode = mode;
    return true;
}

static bool fake_queue(void *context, const struct can_frame *frame)
{
    struct fake_can *fake = context;
    ++fake->queued;
    fake->frame = *frame;
    return !fake->reject_queue;
}

static enum can_result fake_tx_status(void *context)
{
    const struct fake_can *fake = context;
    return fake->tx_result;
}

static void fake_abort(void *context)
{
    struct fake_can *fake = context;
    ++fake->aborts;
}

static enum can_result fake_poll_rx(void *context, struct can_frame *frame)
{
    const struct fake_can *fake = context;
    if (fake->rx_result == CAN_OK) {
        *frame = fake->frame;
        if (fake->corrupt_frame) {
            frame->data[7] ^= 1U;
        }
    }
    return fake->rx_result;
}

static uint32_t fake_now(void *context)
{
    const struct fake_can *fake = context;
    return fake->now;
}

static void fake_idle(void *context)
{
    struct fake_can *fake = context;
    ++fake->now;
}

static struct can_diag make_can(struct fake_can *fake)
{
    static const struct can_diag_ops ops = {
        .start = fake_start, .queue = fake_queue, .tx_status = fake_tx_status,
        .abort_tx = fake_abort, .poll_rx = fake_poll_rx, .now_ms = fake_now,
        .idle = fake_idle
    };
    return (struct can_diag) { .ops = &ops, .context = fake };
}

static void test_normal_transmit(void)
{
    struct fake_can fake = { .tx_result = CAN_PENDING, .now = UINT32_MAX - 5U };
    struct can_diag can = make_can(&fake);
    struct can_frame frame;
    CHECK(can_diag_send_example(&can) == CAN_UNAVAILABLE);
    CHECK(can_diag_receive(&can, &frame) == CAN_UNAVAILABLE);
    CHECK(can_diag_start(&can) == CAN_OK && fake.queued == 0U);

    const uint32_t start = fake.now;
    CHECK(can_diag_send_example(&can) == CAN_TIMEOUT);
    CHECK(fake.now - start == CAN_DIAG_TX_TIMEOUT_MS);
    CHECK(fake.aborts == 1U && fake.normal_starts == 2U && can.mode == CAN_NORMAL);

    fake.tx_result = CAN_OK;
    CHECK(can_diag_send_example(&can) == CAN_OK);
    CHECK(fake.frame.id == 0x078U && !fake.frame.extended && fake.frame.length == 8U);
    for (unsigned int i = 0; i < CAN_DIAG_PAYLOAD_SIZE; ++i) {
        CHECK(fake.frame.data[i] == i * 0x11U);
    }

    fake.reject_queue = true;
    CHECK(can_diag_send_example(&can) == CAN_IO_ERROR);
    CHECK(fake.aborts == 1U && fake.normal_starts == 3U && can.mode == CAN_NORMAL);
}

static void test_loopback(void)
{
    struct fake_can fake = {0};
    struct can_diag can = make_can(&fake);
    CHECK(can_diag_start(&can) == CAN_OK);
    CHECK(can_diag_selftest(&can) == CAN_OK);
    CHECK(fake.loopback_starts == 1U && fake.normal_starts == 2U && can.mode == CAN_NORMAL);

    fake.corrupt_frame = true;
    CHECK(can_diag_selftest(&can) == CAN_BAD_FRAME);
    CHECK(fake.mode == CAN_NORMAL && can.mode == CAN_NORMAL);

    fake.tx_result = CAN_PENDING;
    CHECK(can_diag_selftest(&can) == CAN_TIMEOUT);
    CHECK(fake.aborts == 1U && can.mode == CAN_NORMAL);

    fake.tx_result = CAN_OK;
    fake.rx_result = CAN_PENDING;
    const uint32_t start = fake.now;
    CHECK(can_diag_selftest(&can) == CAN_TIMEOUT);
    CHECK(fake.now - start == CAN_DIAG_RX_TIMEOUT_MS && can.mode == CAN_NORMAL);

    fake.fail_loopback = true;
    CHECK(can_diag_selftest(&can) == CAN_UNAVAILABLE && can.mode == CAN_NORMAL);

    fake.fail_normal_on = fake.normal_starts + 1U;
    CHECK(can_diag_selftest(&can) == CAN_RECOVERY_FAILED);
    CHECK(can.mode == CAN_DOWN && can_diag_send_example(&can) == CAN_UNAVAILABLE);
}

int main(void)
{
    test_normal_transmit();
    test_loopback();
    return EXIT_SUCCESS;
}
