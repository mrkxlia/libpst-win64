# libpst fuzzing harnesses

libFuzzer (clang) harnesses for the untrusted `.pst` parsing paths, plus
a seed corpus. These are exercised by the `security.yml` GitHub Actions
workflow, and can also be run locally.

## Harnesses

- `fuzz_lzfu.c` — drives `pst_lzfu_decompress()` (compressed-RTF expansion,
  `src/lzfu.c`) directly with the raw input bytes. High throughput, no file I/O.
- `fuzz_pst_open.c` — writes each input to a temp file and drives the full
  parse path: `pst_open` → `pst_load_index` → `pst_load_extended_attributes`
  → `pst_parse_item` over every descriptor → attachment assembly and RTF
  decompression, mirroring how `readpst` walks a file.

## Corpus

- `fuzz/corpus/` — seed inputs the fuzzer starts from.
- `../regression/fuzz-corpus/` — pinned reproducers for bugs that were found
  and fixed. The `asan-ubsan` CI job replays these under AddressSanitizer /
  UndefinedBehaviorSanitizer so the fixes stay regression-tested.

## Running locally

Requires clang with the compiler-rt fuzzer/sanitizer runtimes
(`libclang-rt-dev`) and zlib headers.

```sh
# from a configured build tree (so config.h exists):
autoreconf -fiv && ./configure --enable-python=no --enable-dii=no

SAN="-fsanitize=fuzzer,address,undefined"
COMMON="src/debug.c src/libpst.c src/libstrfunc.c src/lzfu.c src/timeconv.c src/vbuf.c"

# lzfu harness
clang -g -O1 -DHAVE_CONFIG_H -I . -I src $SAN fuzz/fuzz_lzfu.c src/lzfu.c src/debug.c -o fuzz_lzfu

# full parser harness
clang -g -O1 -DHAVE_CONFIG_H -I . -I src $SAN fuzz/fuzz_pst_open.c $COMMON -lz -o fuzz_pst_open

# run (cap allocation so attacker-controlled sizes surface as OOM, not corruption)
ASAN_OPTIONS=allocator_may_return_null=1 \
  ./fuzz_lzfu -max_total_time=300 -rss_limit_mb=2560 -malloc_limit_mb=2048 fuzz/corpus
```

## Notes

`pst_malloc()`/`pst_realloc()` call `exit(1)` on allocation failure, so
inputs that request very large buffers show up as libFuzzer OOM rather than
as ASAN reports. Run with `-rss_limit_mb`/`-malloc_limit_mb` to bound this.
