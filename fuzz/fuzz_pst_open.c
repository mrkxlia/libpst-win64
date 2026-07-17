/*
 * libFuzzer harness for the core .pst parsing path. pst_open() only
 * accepts a filename (it calls fopen() internally), so each fuzz input
 * is written to a temporary file and then driven through the same
 * sequence readpst uses: open -> load_index -> load_extended_attributes
 * -> parse every descriptor -> touch attachments and compressed RTF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libpst.h"
#include "lzfu.h"

/* Bound the descriptors visited so a pathological tree cannot make a
 * single fuzz iteration run unboundedly long. */
#define MAX_ITEMS 4096

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char tmpl[] = "/tmp/fuzz_pst_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return 0;
    if (size && write(fd, data, size) != (ssize_t)size) {
        close(fd);
        unlink(tmpl);
        return 0;
    }
    close(fd);

    pst_file pf;
    memset(&pf, 0, sizeof(pf));
    if (pst_open(&pf, tmpl, "UTF-8") == 0) {
        if (pst_load_index(&pf) == 0) {
            pst_load_extended_attributes(&pf);

            int count = 0;
            pst_desc_tree *d_ptr = pf.d_head;
            while (d_ptr && count++ < MAX_ITEMS) {
                pst_item *item = pst_parse_item(&pf, d_ptr, NULL);
                if (item) {
                    if (item->email &&
                        item->email->rtf_compressed.data &&
                        item->email->rtf_compressed.size) {
                        size_t rsz = 0;
                        char *rtf = pst_lzfu_decompress(
                            item->email->rtf_compressed.data,
                            (uint32_t)item->email->rtf_compressed.size, &rsz);
                        free(rtf);
                    }
                    for (pst_item_attach *a = item->attach; a; a = a->next) {
                        pst_binary b = pst_attach_to_mem(&pf, a);
                        free(b.data);
                    }
                    pst_freeItem(item);
                }
                d_ptr = pst_getNextDptr(d_ptr);
            }
        }
        pst_close(&pf);
    }

    unlink(tmpl);
    return 0;
}
