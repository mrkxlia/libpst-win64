/*
 * libFuzzer harness that mirrors the high-level walk performed by the
 * pybind11 binding (python/pybind_libpst.cpp): open -> load_index ->
 * load_extended_attributes -> parse root -> pst_getTopOfFolders -> recurse
 * the folder tree, reading each message's UTF-8 fields and attachment names.
 *
 * This exercises the getTopOfFolders + folder-recursion path, which differs
 * from the flat pst_getNextDptr walk in fuzz_pst_open.c.
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

#define MAX_DEPTH 128

static void touch_string(pst_item *item, pst_string *s) {
    if (s && s->str) {
        pst_convert_utf8_null(item, s);
        if (s->str) { volatile size_t n = strlen(s->str); (void)n; }
    }
}

static void extract(pst_item *item) {
    touch_string(item, &item->subject);
    touch_string(item, &item->body);
    if (item->email) {
        touch_string(item, &item->email->sender_address);
        touch_string(item, &item->email->outlook_sender_name);
        touch_string(item, &item->email->messageid);
    }
    for (pst_item_attach *a = item->attach; a; a = a->next) {
        touch_string(item, &a->filename2);
        touch_string(item, &a->filename1);
        touch_string(item, &a->mimetype);
    }
}

static void collect(pst_file *pf, pst_desc_tree *d_ptr, int depth) {
    if (depth > MAX_DEPTH) return;
    for (; d_ptr; d_ptr = d_ptr->next) {
        if (!d_ptr->desc) continue;
        pst_item *item = pst_parse_item(pf, d_ptr, NULL);
        if (!item) continue;
        if (item->folder && item->file_as.str) {
            touch_string(item, &item->file_as);
            if (d_ptr->child) collect(pf, d_ptr->child, depth + 1);
        } else if (item->type == PST_TYPE_NOTE && item->email) {
            extract(item);
        }
        pst_freeItem(item);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char tmpl[] = "/tmp/fuzz_walk_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return 0;
    if (size && write(fd, data, size) != (ssize_t)size) { close(fd); unlink(tmpl); return 0; }
    close(fd);

    pst_file pf;
    memset(&pf, 0, sizeof(pf));
    if (pst_open(&pf, tmpl, "UTF-8") == 0) {
        if (pst_load_index(&pf) == 0) {
            pst_load_extended_attributes(&pf);
            pst_item *root = pf.d_head ? pst_parse_item(&pf, pf.d_head, NULL) : NULL;
            if (root) {
                pst_desc_tree *top = pst_getTopOfFolders(&pf, root);
                if (top && top->child) collect(&pf, top->child, 0);
                pst_freeItem(root);
            }
        }
        pst_close(&pf);
    }
    unlink(tmpl);
    return 0;
}
