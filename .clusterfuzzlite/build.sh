#!/bin/bash -eu
#
# Build script shared by ClusterFuzzLite and OSS-Fuzz. Compiles the libpst
# fuzz harnesses against the C core with the fuzzing engine and sanitizer
# flags supplied by the environment ($CC/$CFLAGS/$LIB_FUZZING_ENGINE).

# libpst.h wraps every struct (including in-memory trees) in #pragma pack(1),
# so UBSan's alignment check fires on almost every field access on x86_64
# where those loads are safe. Exclude just that check; keep everything else.
export CFLAGS="${CFLAGS:-} -fno-sanitize=alignment"
export CXXFLAGS="${CXXFLAGS:-} -fno-sanitize=alignment"

# Generate config.h via the project's own feature detection.
autoreconf -fiv
./configure --enable-python=no --enable-dii=no

CORE="src/debug.c src/libpst.c src/libstrfunc.c src/lzfu.c src/timeconv.c src/vbuf.c"

# Full parse + folder-walk harnesses (need zlib for compressed blocks).
for h in fuzz_pst_open fuzz_pst_walk; do
  $CC $CFLAGS -DHAVE_CONFIG_H -I. -Isrc \
    $LIB_FUZZING_ENGINE fuzz/$h.c $CORE -lz -o "$OUT/$h"
  cp fuzz/pst.dict "$OUT/$h.dict"
done

# Compressed-RTF (LZFu) harness — only needs lzfu + debug.
$CC $CFLAGS -DHAVE_CONFIG_H -I. -Isrc \
  $LIB_FUZZING_ENGINE fuzz/fuzz_lzfu.c src/lzfu.c src/debug.c -o "$OUT/fuzz_lzfu"
cp fuzz/pst.dict "$OUT/fuzz_lzfu.dict"

# Seed corpora: the adversarial regression inputs, the fuzz seeds, and the
# valid PST fixture (which unlocks the deep parser paths). OSS-Fuzz picks up a
# <harness>_seed_corpus.zip sitting next to each built harness in $OUT.
for h in fuzz_pst_open fuzz_pst_walk; do
  zip -q -j "$OUT/${h}_seed_corpus.zip" \
    fuzz/corpus/* regression/fuzz-corpus/* tests/fixtures/dist-list.pst 2>/dev/null || true
done
zip -q -j "$OUT/fuzz_lzfu_seed_corpus.zip" fuzz/corpus/lzfu_seed 2>/dev/null || true
