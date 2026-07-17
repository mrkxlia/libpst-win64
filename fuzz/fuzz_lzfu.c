/*
 * libFuzzer harness for pst_lzfu_decompress() — the compressed-RTF
 * expansion path (src/lzfu.c). This decompressor consumes an
 * attacker-controlled LZFU header and body straight from a .pst
 * attachment, so it is a prime target for memory-safety fuzzing.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lzfu.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* pst_lzfu_decompress takes a mutable char* and a uint32_t size. */
    if (size > 0xFFFFFFFFu) return 0;

    char *buf = (char *)malloc(size ? size : 1);
    if (!buf) return 0;
    if (size) memcpy(buf, data, size);

    size_t out_size = 0;
    char *out = pst_lzfu_decompress(buf, (uint32_t)size, &out_size);
    free(out);
    free(buf);
    return 0;
}
