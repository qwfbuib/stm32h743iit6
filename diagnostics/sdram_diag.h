
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SHARK_SDRAM_DIAG_H
#define SHARK_SDRAM_DIAG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SHARK_SDRAM_TEST_SIZE (64U * 1024U)

struct sdram_mismatch {
    size_t byte_offset;
    uint32_t expected;
    uint32_t actual;
};

bool sdram_parse_size(const char *text, size_t *bytes);
bool sdram_memory_test(volatile uint32_t *memory, size_t bytes,
                       void (*sync_writes)(void), struct sdram_mismatch *mismatch);

#endif
