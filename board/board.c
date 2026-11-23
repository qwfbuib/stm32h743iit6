
/* SPDX-License-Identifier: Apache-2.0 */
#include "board.h"

#include <rthw.h>
#include <rtthread.h>

#include "sdram_diag.h"

_Static_assert(RT_TICK_PER_SECOND == 1000, "SysTick is shared with the HAL millisecond timer");
_Static_assert((SHARK_SDRAM_TEST_BASE % SHARK_SDRAM_TEST_SIZE) == 0,
               "SDRAM test region must be aligned to its MPU size");
_Static_assert(SHARK_FRAMEBUFFER_SIZE + SHARK_SDRAM_TEST_SIZE <= SHARK_SDRAM_SIZE,
               "SDRAM test region exceeds the physical bank");

extern char __heap_start[];
extern char __heap_end[];
extern const uint32_t g_pfnVectors[];

static volatile bool kernel_tick_enabled;
static bool clock_configured;

static bool wait_for_voltage_scaling(void)
{
    const uint32_t start = HAL_GetTick();

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
        if (HAL_GetTick() - start >= 100U) {
            return false;
        }
    }
    return true;
}

static bool configure_clocks(void)
{
    if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK) {
        return false;
    }

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    if (!wait_for_voltage_scaling()) {
        return false;
    }
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    if (!wait_for_voltage_scaling()) {
        return false;
    }

    RCC_OscInitTypeDef oscillators = {
        .OscillatorType = RCC_OSCILLATORTYPE_HSE,
        .HSEState = RCC_HSE_ON,
        .PLL = {
            .PLLState = RCC_PLL_ON,
            .PLLSource = RCC_PLLSOURCE_HSE,
            .PLLM = 5,
            .PLLN = 192,
            .PLLP = 2,
            .PLLQ = 8,
            .PLLR = 2,
            .PLLRGE = RCC_PLL1VCIRANGE_2,
            .PLLVCOSEL = RCC_PLL1VCOWIDE
        }
    };
    if (HAL_RCC_OscConfig(&oscillators) != HAL_OK) {
        return false;
    }

    RCC_ClkInitTypeDef buses = {
        .ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                     RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                     RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1,
        .SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK,
        .SYSCLKDivider = RCC_SYSCLK_DIV1,
        .AHBCLKDivider = RCC_HCLK_DIV2,
        .APB3CLKDivider = RCC_APB3_DIV2,
        .APB1CLKDivider = RCC_APB1_DIV2,
        .APB2CLKDivider = RCC_APB2_DIV2,
        .APB4CLKDivider = RCC_APB4_DIV2
    };
    if (HAL_RCC_ClockConfig(&buses, FLASH_LATENCY_4) != HAL_OK) {
        return false;
    }

    return SystemCoreClock == SHARK_SYSCLK_HZ &&
           HAL_RCC_GetHCLKFreq() == SHARK_HCLK_HZ &&
           HAL_RCC_GetPCLK1Freq() == SHARK_APB1_HZ;
}

static void configure_mpu(void)
{
    MPU_Region_InitTypeDef region = {
        .Enable = MPU_REGION_ENABLE,
        .Number = MPU_REGION_NUMBER0,
        .BaseAddress = SHARK_SDRAM_TEST_BASE,
        .Size = MPU_REGION_SIZE_64KB,
        .TypeExtField = MPU_TEX_LEVEL1,
        .AccessPermission = MPU_REGION_FULL_ACCESS,
        .DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE,
        .IsShareable = MPU_ACCESS_SHAREABLE,
        .IsCacheable = MPU_ACCESS_NOT_CACHEABLE,
        .IsBufferable = MPU_ACCESS_NOT_BUFFERABLE
    };

    HAL_MPU_Disable();
    HAL_MPU_ConfigRegion(&region);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    SCB_EnableICache();
    SCB_EnableDCache();
}

static void led_init(void)
{
    __HAL_RCC_GPIOI_CLK_ENABLE();
    board_led_set(false);

    GPIO_InitTypeDef pin = {
        .Pin = SHARK_LED_PIN,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW
    };
    HAL_GPIO_Init(SHARK_LED_PORT, &pin);
}

void board_led_set(bool enabled)
{
    HAL_GPIO_WritePin(SHARK_LED_PORT, SHARK_LED_PIN,
                      enabled ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void board_gpio_alternate(GPIO_TypeDef *port, uint32_t pins, uint32_t alternate,
                          uint32_t speed)
{
    GPIO_InitTypeDef pin = {
        .Pin = pins,
        .Mode = GPIO_MODE_AF_PP,
        .Pull = GPIO_NOPULL,
        .Speed = speed,
        .Alternate = alternate
    };
    HAL_GPIO_Init(port, &pin);
}

bool board_clock_ready(void)
{
    return clock_configured;
}

void SysTick_Handler(void)
{
    HAL_IncTick();

    if (kernel_tick_enabled) {
        rt_interrupt_enter();
        rt_tick_increase();
        rt_interrupt_leave();
    }
}

static int board_runtime_init(void)
{
    kernel_tick_enabled = true;
    board_console_start_rx();
    return RT_EOK;
}
INIT_PREV_EXPORT(board_runtime_init);

void rt_hw_board_init(void)
{
    SCB->VTOR = (uint32_t)(uintptr_t)g_pfnVectors;
    __DSB();
    __ISB();
    configure_mpu();
    led_init();

    /* RT-Thread starts with IRQs masked. HAL clock timeouts need SysTick. */
    __enable_irq();
    clock_configured = HAL_Init() == HAL_OK && configure_clocks();
    const bool console_ready = board_console_init();
    __disable_irq();

    rt_system_heap_init(__heap_start, __heap_end);
    rt_components_board_init();

    if (!console_ready) {
        board_led_set(true);
        for (;;) {
            __WFI();
        }
    }
}
