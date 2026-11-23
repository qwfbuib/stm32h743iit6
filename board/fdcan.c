
/* SPDX-License-Identifier: Apache-2.0 */
#include "board.h"

#include <rtthread.h>
#include <string.h>

#define NOMINAL_PRESCALER 24U
#define NOMINAL_SEGMENT_1 35U
#define NOMINAL_SEGMENT_2 4U

_Static_assert(SHARK_FDCAN_CLOCK_HZ ==
                   SHARK_CAN_BITRATE * NOMINAL_PRESCALER *
                       (1U + NOMINAL_SEGMENT_1 + NOMINAL_SEGMENT_2),
               "FDCAN timing does not produce the board bitrate");

struct fdcan_port {
    FDCAN_HandleTypeDef handle;
    uint32_t tx_buffer;
};

static struct fdcan_port fdcan;

static bool fdcan_start(void *context, enum can_mode mode)
{
    struct fdcan_port *port = context;
    RCC_PeriphCLKInitTypeDef clock = {
        .PeriphClockSelection = RCC_PERIPHCLK_FDCAN,
        .FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL
    };
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) {
        return false;
    }
    PLL1_ClocksTypeDef pll;
    HAL_RCCEx_GetPLL1ClockFreq(&pll);
    if (pll.PLL1_Q_Frequency != SHARK_FDCAN_CLOCK_HZ) {
        return false;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_FDCAN_CLK_ENABLE();

    /* FDCAN1 owns the peripheral and its message RAM in this firmware. */
    __HAL_RCC_FDCAN_FORCE_RESET();
    __HAL_RCC_FDCAN_RELEASE_RESET();
    *port = (struct fdcan_port) {
        .handle = {
            .Instance = FDCAN1,
            .Init = {
                .FrameFormat = FDCAN_FRAME_CLASSIC,
                .Mode = mode == CAN_LOOPBACK ? FDCAN_MODE_INTERNAL_LOOPBACK : FDCAN_MODE_NORMAL,
                .AutoRetransmission = DISABLE,
                .TransmitPause = DISABLE,
                .ProtocolException = DISABLE,
                .NominalPrescaler = NOMINAL_PRESCALER,
                .NominalSyncJumpWidth = NOMINAL_SEGMENT_2,
                .NominalTimeSeg1 = NOMINAL_SEGMENT_1,
                .NominalTimeSeg2 = NOMINAL_SEGMENT_2,
                .DataPrescaler = 1U,
                .DataSyncJumpWidth = 1U,
                .DataTimeSeg1 = 1U,
                .DataTimeSeg2 = 1U,
                .RxFifo0ElmtsNbr = 8U,
                /* The HAL maps a classic frame's raw DLC above 8 to FD lengths. */
                .RxFifo0ElmtSize = FDCAN_DATA_BYTES_64,
                .TxFifoQueueElmtsNbr = 1U,
                .TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION,
                .TxElmtSize = FDCAN_DATA_BYTES_8
            }
        }
    };
    board_gpio_alternate(SHARK_CAN_PORT, SHARK_CAN_PINS, GPIO_AF9_FDCAN1,
                         GPIO_SPEED_FREQ_VERY_HIGH);

    return HAL_FDCAN_Init(&port->handle) == HAL_OK &&
           HAL_FDCAN_ConfigGlobalFilter(&port->handle, FDCAN_ACCEPT_IN_RX_FIFO0,
                                        FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT_REMOTE,
                                        FDCAN_REJECT_REMOTE) == HAL_OK &&
           HAL_FDCAN_Start(&port->handle) == HAL_OK;
}

static bool fdcan_queue(void *context, const struct can_frame *frame)
{
    struct fdcan_port *port = context;
    FDCAN_TxHeaderTypeDef header = {
        .Identifier = frame->id,
        .IdType = FDCAN_STANDARD_ID,
        .TxFrameType = FDCAN_DATA_FRAME,
        .DataLength = FDCAN_DLC_BYTES_8,
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch = FDCAN_BRS_OFF,
        .FDFormat = FDCAN_CLASSIC_CAN,
        .TxEventFifoControl = FDCAN_NO_TX_EVENTS
    };

    uint8_t data[CAN_DIAG_PAYLOAD_SIZE];
    memcpy(data, frame->data, sizeof(data));
    if (HAL_FDCAN_AddMessageToTxFifoQ(&port->handle, &header, data) != HAL_OK) {
        return false;
    }

    port->tx_buffer = HAL_FDCAN_GetLatestTxFifoQRequestBuffer(&port->handle);
    return true;
}

static enum can_result fdcan_tx_status(void *context)
{
    const struct fdcan_port *port = context;
    FDCAN_GlobalTypeDef *regs = port->handle.Instance;

    if ((regs->TXBTO & port->tx_buffer) != 0U) {
        return CAN_OK;
    }
    if ((regs->TXBCF & port->tx_buffer) != 0U || (regs->PSR & FDCAN_PSR_BO) != 0U) {
        return CAN_IO_ERROR;
    }
    return CAN_PENDING;
}

static void fdcan_abort(void *context)
{
    struct fdcan_port *port = context;
    if (HAL_FDCAN_IsTxBufferMessagePending(&port->handle, port->tx_buffer) != 0U) {
        (void)HAL_FDCAN_AbortTxRequest(&port->handle, port->tx_buffer);
    }
}

static enum can_result fdcan_poll_rx(void *context, struct can_frame *frame)
{
    struct fdcan_port *port = context;
    if (HAL_FDCAN_GetRxFifoFillLevel(&port->handle, FDCAN_RX_FIFO0) == 0U) {
        return CAN_PENDING;
    }

    FDCAN_RxHeaderTypeDef header;
    /* HAL uses the encoded DLC when copying message RAM. */
    uint8_t data[64];
    if (HAL_FDCAN_GetRxMessage(&port->handle, FDCAN_RX_FIFO0, &header, data) != HAL_OK) {
        return CAN_IO_ERROR;
    }
    if (header.FDFormat != FDCAN_CLASSIC_CAN || header.RxFrameType != FDCAN_DATA_FRAME) {
        return CAN_BAD_FRAME;
    }

    const uint32_t dlc = header.DataLength >> 16U;
    *frame = (struct can_frame) {
        .id = header.Identifier,
        .extended = header.IdType == FDCAN_EXTENDED_ID,
        .length = (uint8_t)(dlc > CAN_DIAG_PAYLOAD_SIZE ? CAN_DIAG_PAYLOAD_SIZE : dlc)
    };
    memcpy(frame->data, data, frame->length);
    return CAN_OK;
}

static uint32_t fdcan_now(void *context)
{
    (void)context;
    return HAL_GetTick();
}

static void fdcan_idle(void *context)
{
    (void)context;
    rt_thread_mdelay(1);
}

void board_can_init(struct can_diag *can)
{
    static const struct can_diag_ops operations = {
        .start = fdcan_start,
        .queue = fdcan_queue,
        .tx_status = fdcan_tx_status,
        .abort_tx = fdcan_abort,
        .poll_rx = fdcan_poll_rx,
        .now_ms = fdcan_now,
        .idle = fdcan_idle
    };
    *can = (struct can_diag) { .ops = &operations, .context = &fdcan };
}
