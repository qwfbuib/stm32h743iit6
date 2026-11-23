
/* SPDX-License-Identifier: Apache-2.0 */
#include "sdram_diag.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static uint32_t memory[SHARK_SDRAM_TEST_SIZE / sizeof(uint32_t) + 2U];
static unsigned int sync_count;
static unsigned int corrupt_on_pass;

static void sync_test_memory(void)
{
    ++sync_count;
    if (sync_count == corrupt_on_pass) {
        memory[2] ^= 0x100U;
    }
}

static void test_size_parsing(void)
{
    size_t bytes = 0;
    CHECK(sdram_parse_size("4", &bytes) && bytes == 4U);
    CHECK(sdram_parse_size("65536", &bytes) && bytes == SHARK_SDRAM_TEST_SIZE);

    const char *const invalid[] = {
        "", "0", "3", "5", "65535", "65540", "-4", "+4",
        "4x", " 4", "0x4", "4294967300", "99999999999999999999999999"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(!sdram_parse_size(invalid[i], &bytes));
        CHECK(bytes == SHARK_SDRAM_TEST_SIZE);
    }
}

static void test_memory_patterns(void)
{
    struct sdram_mismatch mismatch;
    const size_t last = sizeof(memory) / sizeof(memory[0]) - 1U;
    memory[0] = 0x12345678U;
    memory[last] = 0x89abcdefU;

    CHECK(!sdram_memory_test(&memory[1], 3U, sync_test_memory, &mismatch));
    CHECK(sync_count == 0U);
    CHECK(sdram_memory_test(&memory[1], SHARK_SDRAM_TEST_SIZE, sync_test_memory, &mismatch));
    CHECK(sync_count == 2U);
    CHECK(memory[1] == 0xaaaa5555U);
    CHECK(memory[2] == (0xaaaa5555U ^ 1U));
    CHECK(memory[0] == 0x12345678U && memory[last] == 0x89abcdefU);

    sync_count = 0;
    corrupt_on_pass = 2;
    CHECK(!sdram_memory_test(&memory[1], 8U, sync_test_memory, &mismatch));
    CHECK(sync_count == 2U && mismatch.byte_offset == 4U);
    CHECK(mismatch.expected == (0xaaaa5555U ^ 1U));
    CHECK(mismatch.actual == (mismatch.expected ^ 0x100U));
}

int main(void)
{
    test_size_parsing();
    test_memory_patterns();
    return EXIT_SUCCESS;
}
