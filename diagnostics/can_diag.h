
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SHARK_CAN_DIAG_H
#define SHARK_CAN_DIAG_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_DIAG_PAYLOAD_SIZE 8U
#define CAN_DIAG_TX_TIMEOUT_MS 100U
#define CAN_DIAG_RX_TIMEOUT_MS 1000U

enum can_result {
    CAN_OK, CAN_PENDING, CAN_TIMEOUT, CAN_IO_ERROR, CAN_UNAVAILABLE,
    CAN_BAD_FRAME, CAN_RECOVERY_FAILED
};

enum can_mode { CAN_DOWN, CAN_NORMAL, CAN_LOOPBACK };

struct can_frame {
    uint32_t id;
    bool extended;
    uint8_t length;
    uint8_t data[CAN_DIAG_PAYLOAD_SIZE];
};

struct can_diag_ops {
    bool (*start)(void *context, enum can_mode mode);
    bool (*queue)(void *context, const struct can_frame *frame);
    enum can_result (*tx_status)(void *context);
    void (*abort_tx)(void *context);
    enum can_result (*poll_rx)(void *context, struct can_frame *frame);
    uint32_t (*now_ms)(void *context);
    void (*idle)(void *context);
};

struct can_diag {
    const struct can_diag_ops *ops;
    void *context;
    enum can_mode mode;
};

enum can_result can_diag_start(struct can_diag *can);
enum can_result can_diag_send_example(struct can_diag *can);
enum can_result can_diag_receive(struct can_diag *can, struct can_frame *frame);
enum can_result can_diag_selftest(struct can_diag *can);
const char *can_diag_result_text(enum can_result result);

#endif
