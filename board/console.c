
/* SPDX-License-Identifier: Apache-2.0 */
#include "board.h"

#include <rthw.h>
#include <rtthread.h>

#define UART_RX_CAPACITY 128U

static UART_HandleTypeDef uart;
static bool uart_configured;
static char rx_buffer[UART_RX_CAPACITY];
static volatile unsigned int rx_read;
static volatile unsigned int rx_write;
static struct rt_semaphore rx_available;

bool board_console_init(void)
{
    RCC_PeriphCLKInitTypeDef clock = {
        .PeriphClockSelection = RCC_PERIPHCLK_USART2,
        .Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1
    };
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) {
        return false;
    }

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    board_gpio_alternate(SHARK_UART_PORT, SHARK_UART_PINS, GPIO_AF7_USART2,
                         GPIO_SPEED_FREQ_LOW);

    uart.Instance = USART2;
    uart.Init = (UART_InitTypeDef) {
        .BaudRate = SHARK_UART_BAUD,
        .WordLength = UART_WORDLENGTH_8B,
        .StopBits = UART_STOPBITS_1,
        .Parity = UART_PARITY_NONE,
        .Mode = UART_MODE_TX_RX,
        .HwFlowCtl = UART_HWCONTROL_NONE,
        .OverSampling = UART_OVERSAMPLING_16,
        .ClockPrescaler = UART_PRESCALER_DIV1
    };
    uart_configured = HAL_UART_Init(&uart) == HAL_OK;
    return uart_configured;
}

void board_console_start_rx(void)
{
    rt_sem_init(&rx_available, "uart2rx", 0, RT_IPC_FLAG_FIFO);
    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_ClearPendingIRQ(USART2_IRQn);
    SET_BIT(USART2->CR1, USART_CR1_RXNEIE_RXFNEIE);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void USART2_IRQHandler(void)
{
    rt_interrupt_enter();

    const uint32_t status = USART2->ISR;
    if ((status & USART_ISR_RXNE_RXFNE) != 0U) {
        const char received = (char)USART2->RDR;
        const unsigned int next = (rx_write + 1U) % UART_RX_CAPACITY;
        if (next != rx_read && (status & (USART_ISR_FE | USART_ISR_NE)) == 0U) {
            rx_buffer[rx_write] = received;
            rx_write = next;
            rt_sem_release(&rx_available);
        }
    }
    if ((status & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) != 0U) {
        USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    }

    rt_interrupt_leave();
}

char rt_hw_console_getchar(void)
{
    rt_sem_take(&rx_available, RT_WAITING_FOREVER);
    const char received = rx_buffer[rx_read];
    rx_read = (rx_read + 1U) % UART_RX_CAPACITY;
    return received;
}

static bool console_put(char ch)
{
    /* The kernel prints its banner with IRQs masked, before RT-Thread ticks. */
    for (uint32_t tries = SystemCoreClock / 1000U; tries != 0U; --tries) {
        if ((USART2->ISR & USART_ISR_TXE_TXFNF) != 0U) {
            USART2->TDR = (uint8_t)ch;
            return true;
        }
    }
    return false;
}

void rt_hw_console_output(const char *text)
{
    if (!uart_configured) {
        return;
    }

    char previous = 0;
    for (; *text != '\0'; ++text) {
        if ((*text == '\n' && previous != '\r' && !console_put('\r')) ||
            !console_put(*text)) {
            return;
        }
        previous = *text;
    }
}
