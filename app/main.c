
/* SPDX-License-Identifier: Apache-2.0 */
#include "board.h"
#include "sdram_diag.h"

#include <finsh.h>
#include <rtthread.h>

static bool sdram_ready;
static struct can_diag can;

static int sdram(int argc, char **argv)
{
    size_t bytes;
    if (argc != 2 || !sdram_parse_size(argv[1], &bytes)) {
        rt_kprintf("Usage: sdram <bytes>, multiple of 4, 4..65536\n");
        return -RT_EINVAL;
    }
    if (!sdram_ready) {
        rt_kprintf("SDRAM unavailable\n");
        return -RT_ERROR;
    }

    struct sdram_mismatch mismatch;
    volatile uint32_t *const scratch = (volatile uint32_t *)SHARK_SDRAM_TEST_BASE;
    if (!sdram_memory_test(scratch, bytes, board_sdram_sync, &mismatch)) {
        rt_kprintf("SDRAM FAIL offset=%u expected=%08x actual=%08x\n",
                   (unsigned int)mismatch.byte_offset, (unsigned int)mismatch.expected,
                   (unsigned int)mismatch.actual);
        return -RT_ERROR;
    }

    rt_kprintf("SDRAM PASS %u bytes, 2 patterns\n", (unsigned int)bytes);
    return RT_EOK;
}
MSH_CMD_EXPORT(sdram, Test external SDRAM);

static int report_can(const char *command, enum can_result result)
{
    rt_kprintf("%s: %s\n", command, can_diag_result_text(result));
    return result == CAN_OK ? RT_EOK : -RT_ERROR;
}

static int cantx(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        rt_kprintf("Usage: cantx\n");
        return -RT_EINVAL;
    }
    return report_can("cantx", can_diag_send_example(&can));
}
MSH_CMD_EXPORT(cantx, Send the example CAN frame);

static int can_rx(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        rt_kprintf("Usage: can_rx\n");
        return -RT_EINVAL;
    }

    struct can_frame frame;
    const enum can_result result = can_diag_receive(&can, &frame);
    if (result != CAN_OK) {
        return report_can("can_rx", result);
    }
    rt_kprintf("CAN %s id=0x%08x len=%u",
               frame.extended ? "extended" : "standard", (unsigned int)frame.id,
               (unsigned int)frame.length);
    for (unsigned int i = 0; i < frame.length; ++i) {
        rt_kprintf(" %02x", (unsigned int)frame.data[i]);
    }
    rt_kprintf("\n");
    return RT_EOK;
}
MSH_CMD_EXPORT(can_rx, Receive one CAN frame);

static int can_selftest(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        rt_kprintf("Usage: can_selftest\n");
        return -RT_EINVAL;
    }
    if (!board_clock_ready()) {
        return report_can("can_selftest", CAN_UNAVAILABLE);
    }
    return report_can("can_selftest", can_diag_selftest(&can));
}
MSH_CMD_EXPORT(can_selftest, Test internal CAN loopback);

int main(void)
{
    rt_kprintf("IoT Shark V2 diagnostics\n");
    if (!board_clock_ready()) {
        rt_kprintf("Clock: 480 MHz configuration failed; external diagnostics disabled\n");
    }

    sdram_ready = board_sdram_init();
    rt_kprintf("SDRAM init: %s\n", sdram_ready ? "ok" : "failed");

    board_can_init(&can);
    const enum can_result can_ready = board_clock_ready() ? can_diag_start(&can) : CAN_UNAVAILABLE;
    report_can("FDCAN1 init", can_ready);

    for (;;) {
        board_led_set(true);
        rt_thread_mdelay(50);
        board_led_set(false);
        rt_thread_mdelay(950);
    }
}
