
/* SPDX-License-Identifier: Apache-2.0 */
#include "board.h"

static SDRAM_HandleTypeDef sdram;

static void configure_pins(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    board_gpio_alternate(GPIOC, GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3,
                         GPIO_AF12_FMC, GPIO_SPEED_FREQ_VERY_HIGH);
    board_gpio_alternate(GPIOD, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
                         GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15,
                         GPIO_AF12_FMC, GPIO_SPEED_FREQ_VERY_HIGH);
    board_gpio_alternate(GPIOE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 |
                         GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                         GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
                         GPIO_AF12_FMC, GPIO_SPEED_FREQ_VERY_HIGH);
    board_gpio_alternate(GPIOF, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                         GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 |
                         GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
                         GPIO_AF12_FMC, GPIO_SPEED_FREQ_VERY_HIGH);
    board_gpio_alternate(GPIOG, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 |
                         GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_15,
                         GPIO_AF12_FMC, GPIO_SPEED_FREQ_VERY_HIGH);
}

static bool send_command(uint32_t mode, uint32_t refresh_count, uint32_t mode_register)
{
    FMC_SDRAM_CommandTypeDef command = {
        .CommandMode = mode,
        .CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1,
        .AutoRefreshNumber = refresh_count,
        .ModeRegisterDefinition = mode_register
    };
    return HAL_SDRAM_SendCommand(&sdram, &command, 100U) == HAL_OK;
}

bool board_sdram_init(void)
{
    if (!board_clock_ready()) {
        return false;
    }

    RCC_PeriphCLKInitTypeDef clock = {
        .PeriphClockSelection = RCC_PERIPHCLK_FMC,
        .FmcClockSelection = RCC_FMCCLKSOURCE_D1HCLK
    };
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) {
        return false;
    }
    __HAL_RCC_FMC_CLK_ENABLE();
    configure_pins();

    sdram.Instance = FMC_SDRAM_DEVICE;
    sdram.Init = (FMC_SDRAM_InitTypeDef) {
        .SDBank = FMC_SDRAM_BANK1,
        .ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_9,
        .RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_13,
        .MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16,
        .InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4,
        .CASLatency = FMC_SDRAM_CAS_LATENCY_3,
        .WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE,
        .SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2,
        .ReadBurst = FMC_SDRAM_RBURST_ENABLE,
        .ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1
    };
    FMC_SDRAM_TimingTypeDef timings = {
        .LoadToActiveDelay = 2,
        .ExitSelfRefreshDelay = 7,
        .SelfRefreshTime = 4,
        .RowCycleDelay = 7,
        .WriteRecoveryTime = 2,
        .RPDelay = 2,
        .RCDDelay = 2
    };
    if (HAL_SDRAM_Init(&sdram, &timings) != HAL_OK ||
        !send_command(FMC_SDRAM_CMD_CLK_ENABLE, 1U, 0U)) {
        return false;
    }

    HAL_Delay(1U);

    /* Sequential single-location read/write bursts, CAS latency 3. */
    const uint32_t mode_register = 0x0230U;
    return send_command(FMC_SDRAM_CMD_PALL, 1U, 0U) &&
           send_command(FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8U, 0U) &&
           send_command(FMC_SDRAM_CMD_LOAD_MODE, 1U, mode_register) &&
           HAL_SDRAM_ProgramRefreshRate(&sdram, 918U) == HAL_OK;
}

void board_sdram_sync(void)
{
    __DSB();
}
