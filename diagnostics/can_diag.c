
/* SPDX-License-Identifier: Apache-2.0 */
#include "can_diag.h"

#include <string.h>

static const struct can_frame example_frame = {
    .id = 0x078U,
    .length = CAN_DIAG_PAYLOAD_SIZE,
    .data = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 }
};

static bool start_mode(struct can_diag *can, enum can_mode mode)
{
    can->mode = CAN_DOWN;
    if (!can->ops->start(can->context, mode)) {
        return false;
    }

    can->mode = mode;
    return true;
}

enum can_result can_diag_start(struct can_diag *can)
{
    return start_mode(can, CAN_NORMAL) ? CAN_OK : CAN_UNAVAILABLE;
}

static enum can_result transmit(struct can_diag *can)
{
    if (!can->ops->queue(can->context, &example_frame)) {
        return CAN_IO_ERROR;
    }

    const uint32_t start = can->ops->now_ms(can->context);
    do {
        const enum can_result result = can->ops->tx_status(can->context);
        if (result != CAN_PENDING) {
            if (result != CAN_OK) {
                can->ops->abort_tx(can->context);
            }
            return result;
        }
        can->ops->idle(can->context);
    } while (can->ops->now_ms(can->context) - start < CAN_DIAG_TX_TIMEOUT_MS);

    can->ops->abort_tx(can->context);
    return CAN_TIMEOUT;
}

enum can_result can_diag_send_example(struct can_diag *can)
{
    if (can->mode != CAN_NORMAL) {
        return CAN_UNAVAILABLE;
    }

    const enum can_result result = transmit(can);
    if (result != CAN_OK && !start_mode(can, CAN_NORMAL)) {
        return CAN_RECOVERY_FAILED;
    }
    return result;
}

enum can_result can_diag_receive(struct can_diag *can, struct can_frame *frame)
{
    if (can->mode == CAN_DOWN) {
        return CAN_UNAVAILABLE;
    }

    const uint32_t start = can->ops->now_ms(can->context);
    do {
        const enum can_result result = can->ops->poll_rx(can->context, frame);
        if (result != CAN_PENDING) {
            return result;
        }
        can->ops->idle(can->context);
    } while (can->ops->now_ms(can->context) - start < CAN_DIAG_RX_TIMEOUT_MS);

    return CAN_TIMEOUT;
}

enum can_result can_diag_selftest(struct can_diag *can)
{
    enum can_result result = CAN_UNAVAILABLE;

    if (start_mode(can, CAN_LOOPBACK)) {
        result = transmit(can);
        if (result == CAN_OK) {
            struct can_frame received;
            result = can_diag_receive(can, &received);
            if (result == CAN_OK &&
                (received.id != example_frame.id || received.extended ||
                 received.length != example_frame.length ||
                 memcmp(received.data, example_frame.data, received.length) != 0)) {
                result = CAN_BAD_FRAME;
            }
        }
    }

    return start_mode(can, CAN_NORMAL) ? result : CAN_RECOVERY_FAILED;
}

const char *can_diag_result_text(enum can_result result)
{
    switch (result) {
    case CAN_OK: return "ok";
    case CAN_PENDING: return "pending";
    case CAN_TIMEOUT: return "timeout";
    case CAN_IO_ERROR: return "controller or bus error";
    case CAN_UNAVAILABLE: return "controller unavailable";
    case CAN_BAD_FRAME: return "unexpected loopback frame";
    case CAN_RECOVERY_FAILED: return "failed to restore normal mode";
    }
    return "unknown error";
}
