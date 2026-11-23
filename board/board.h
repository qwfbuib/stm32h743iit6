
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SHARK_BOARD_H
#define SHARK_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "can_diag.h"
#include "stm32h7xx_hal.h"

#define SHARK_SYSCLK_HZ 480000000UL
#define SHARK_HCLK_HZ 240000000UL
#define SHARK_APB1_HZ 120000000UL
#define SHARK_FDCAN_CLOCK_HZ 120000000UL
#define SHARK_CAN_BITRATE 125000UL

#define SHARK_LED_PORT GPIOI
#define SHARK_LED_PIN GPIO_PIN_1
#define SHARK_UART_PORT GPIOD
#define SHARK_UART_PINS (GPIO_PIN_5 | GPIO_PIN_6)
#define SHARK_UART_BAUD 115200U
#define SHARK_CAN_PORT GPIOB
#define SHARK_CAN_PINS (GPIO_PIN_8 | GPIO_PIN_9)

#define SHARK_SDRAM_BASE 0xc0000000UL
#define SHARK_SDRAM_SIZE (32UL * 1024UL * 1024UL)
#define SHARK_FRAMEBUFFER_SIZE (2UL * 1024UL * 1024UL)
#define SHARK_SDRAM_TEST_BASE (SHARK_SDRAM_BASE + SHARK_FRAMEBUFFER_SIZE)

void board_led_set(bool enabled);
void board_gpio_alternate(GPIO_TypeDef *port, uint32_t pins, uint32_t alternate,
                          uint32_t speed);
bool board_console_init(void);
void board_console_start_rx(void);
bool board_sdram_init(void);
void board_sdram_sync(void);
void board_can_init(struct can_diag *can);
bool board_clock_ready(void);

#endif
