
/* SPDX-License-Identifier: Apache-2.0 */
#include "sdram_diag.h"

static bool valid_size(size_t bytes)
{
    return bytes >= sizeof(uint32_t) && bytes <= SHARK_SDRAM_TEST_SIZE &&
           (bytes % sizeof(uint32_t)) == 0U;
}

bool sdram_parse_size(const char *text, size_t *bytes)
{
    size_t value = 0;

    for (; *text != '\0'; ++text) {
        if (*text < '0' || *text > '9' || value > SHARK_SDRAM_TEST_SIZE / 10U) {
            return false;
        }
        value = value * 10U + (unsigned int)(*text - '0');
    }
    if (!valid_size(value)) {
        return false;
    }

    *bytes = value;
    return true;
}

bool sdram_memory_test(volatile uint32_t *memory, size_t bytes,
                       void (*sync_writes)(void), struct sdram_mismatch *mismatch)
{
    if (!valid_size(bytes)) {
        return false;
    }

    const uint32_t patterns[] = { UINT32_C(0x5555aaaa), UINT32_C(0xaaaa5555) };
    for (size_t pass = 0; pass < sizeof(patterns) / sizeof(patterns[0]); ++pass) {
        const size_t words = bytes / sizeof(*memory);
        for (size_t word = 0; word < words; ++word) {
            memory[word] = patterns[pass] ^ (uint32_t)word;
        }
        sync_writes();

        for (size_t word = 0; word < words; ++word) {
            const uint32_t expected = patterns[pass] ^ (uint32_t)word;
            const uint32_t actual = memory[word];
            if (actual != expected) {
                *mismatch = (struct sdram_mismatch) {
                    .byte_offset = word * sizeof(*memory),
                    .expected = expected,
                    .actual = actual
                };
                return false;
            }
        }
    }
    return true;
}
