/* Regression test for the PST_LE_GET_* little-endian accessors in
 * src/define.h. These read little-endian integers a byte at a time; each byte
 * must be widened to the target unsigned type *before* being shifted. If a
 * byte is left to promote to (signed) int, `byte << 24` overflows int for
 * bytes >= 0x80 and `byte << 32..56` shifts past the width of int — both
 * undefined behavior. This test both checks the returned values and, when
 * compiled with -fsanitize=undefined -fno-sanitize-recover=undefined, fails
 * hard if any shift is UB.
 *
 * Build (as the CI does):
 *   clang -fsanitize=undefined -fno-sanitize-recover=undefined \
 *         -I . -I src tests/test_le_macros.c -o test_le_macros && ./test_le_macros
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Pull in only the accessor macros, not the whole libpst header (which needs
 * config.h and drags in packed structs). They depend on nothing but the
 * standard integer types. */
#include "define.h"

static int failures = 0;

#define CHECK(expr, expected)                                                  \
    do {                                                                       \
        unsigned long long got = (unsigned long long)(expr);                   \
        unsigned long long exp = (unsigned long long)(expected);               \
        if (got != exp) {                                                      \
            fprintf(stderr, "FAIL: %s = 0x%llx, expected 0x%llx\n",            \
                    #expr, got, exp);                                          \
            failures++;                                                        \
        }                                                                      \
    } while (0)

int main(void)
{
    /* High bit set in the most-significant byte of every width — the case
     * that trips signed-shift UB. */
    const uint8_t b[8] = { 0x11, 0x22, 0x33, 0x84, 0x55, 0x66, 0x77, 0x88 };

    CHECK(PST_LE_GET_UINT8(b),  0x11u);
    CHECK(PST_LE_GET_INT8(b),   0x11);

    CHECK(PST_LE_GET_UINT16(b), 0x2211u);
    CHECK(PST_LE_GET_INT16(b),  (int16_t)0x2211);

    CHECK(PST_LE_GET_UINT32(b), 0x84332211u);
    CHECK(PST_LE_GET_INT32(b),  (int32_t)0x84332211u);

    CHECK(PST_LE_GET_UINT64(b), 0x8877665584332211ull);
    CHECK(PST_LE_GET_INT64(b),  (int64_t)0x8877665584332211ull);

    /* A second vector: 0x80 in the top byte of the 32-bit read (the exact
     * shape the fuzzer hit in pst_load_extended_attributes). */
    const uint8_t c[4] = { 0x00, 0x00, 0x00, 0x80 };
    CHECK(PST_LE_GET_UINT32(c), 0x80000000u);

    if (failures) {
        fprintf(stderr, "%d LE-macro check(s) failed\n", failures);
        return 1;
    }
    printf("all LE-macro checks passed\n");
    return 0;
}
