# Upstream patches

Memory-safety and robustness fixes this fork carries on top of
[pst-format/libpst](https://github.com/pst-format/libpst), prepared for
contribution upstream. Every fix is on the read/parse path (crafted `.pst`
input) and preserves behaviour on valid files.

Reproducers referenced below live under `../regression/fuzz-corpus/` and are
replayed under ASAN/UBSan by `.github/workflows/security.yml`.

## Patch sets

* **`0001-tranche-c-memory-safety.patch`** — the batch found once the fuzzers
  were seeded with a valid PST and a format dictionary (they reach the
  extended-attribute / message-store / type-3 sub-block parsers for the first
  time). Regenerate with `git diff <base>..HEAD -- src/`.

## Fix catalog

| Area | Function / file | Class | Reproducer |
| --- | --- | --- | --- |
| Extended attributes | `pst_load_extended_attributes` (libpst.c) | OOB read — attacker-controlled length prefix used unchecked as a memcpy size (~2 GB read) | `oob-xattrib-header-length` |
| Extended attributes | `pst_load_extended_attributes` (libpst.c) | leak — `id2_head` not freed on the "no buffer" early return | — |
| Extended attributes | record loop bound (libpst.c) | OOB read — loop advanced 8 bytes/iteration with only a `< bsize` guard | — |
| Row matrix | `pst_parse_block` `ind2_ptr` (libpst.c) | UB — `NULL + rec_size` pointer arithmetic for 0xbcec blocks | (valid fixture) |
| Raw subject | `pst_parse_block` case 0x0037 (libpst.c) | UB — `data += 0` on a NULL element pointer | — |
| Type-3 sub-blocks | `pst_parse_block` / `freeall` (libpst.c) | SEGV — `malloc`'d sub-block array freed while partly uninitialized (now `calloc`) | `segv-type3-uninitialized-subblock-free` |
| MAPI copy | `LIST_COPY` base macro (libpst.c) | UB — `memcpy` from a NULL element data pointer | `null-data-list-copy-entryid` |
| String compare | `pst_stricmp` / `pst_strincmp` (libpst.c) | NULL deref on absent class field / failed `strdup` | — |
| Little-endian accessors | `PST_LE_GET_*` (define.h) | signed-shift UB (`byte << 24/32..56`) | `tests/test_le_macros.c` |
| vbuf | `pst_vbset` / `pst_vbappend` (vbuf.c) | UB — NULL source passed to `memcpy` | — |
| MAPI elements | `pst_parse_block` / `pst_process` (libpst.c) | NULL deref — element count left at the claimed total when the element list is truncated, so `pst_process` dereferences NULL tail slots | `pst_process_null_mapi_element` |
| Output paths | `write_separate_attachment` (pst2dii.cpp.in) | Path traversal — attacker-controlled attachment filename concatenated into the `fopen(..., "wb")` path unsanitized (same class as the readpst fix already merged) | — |
| PDF naming | `open_pdf` (pst2dii.cpp.in) | Off-by-one — `pdf_name` allocated without room for the NUL (`snprintf(0,0,...)` excludes it), truncating the `.pdf` extension | — |

Earlier hardening already merged on this fork (RCE-class heap overflow in
`pst_read_block_size`/`pst_ff_getIDblock`, `LIST_COPY_*` type-confusion OOB
reads, block-offset bounds, `readpst` path-traversal / MIME-header
injection) is likewise upstreamable; see the project git history
(`git log -- src/`).

## Submitting upstream

```sh
git remote add upstream https://github.com/pst-format/libpst
git fetch upstream
git checkout -b memory-safety upstream/master
git apply upstream-patches/0001-tranche-c-memory-safety.patch
# review, split per-fix as the maintainer prefers, attach reproducers
```
