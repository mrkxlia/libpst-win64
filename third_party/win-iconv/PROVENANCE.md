# Vendored: win-iconv

`win_iconv.c` and `iconv.h` are vendored verbatim from the win-iconv project,
an iconv implementation built on the Win32 API. They are used **only** when
building the `libpst_py` Python extension for Windows (MSVC), where the C
runtime provides no `iconv`. On Linux and macOS the system/libc iconv is used
instead and these files are not compiled.

* **Source:** https://github.com/win-iconv/win-iconv (`win_iconv.c`, `iconv.h`)
* **Author:** Yukihiro Nakadaira
* **License:** Public domain (stated in the project's `readme.txt`, reproduced
  here as `readme.txt`: "win_iconv is placed in the public domain."). Public
  domain is compatible with libpst's GPL-2.0-or-later.

Only the two source files needed to build are vendored; no modifications are
made. See `CMakeLists.txt` for how they are wired into the Windows build.

## Pinned content (integrity)

SHA-256 of the vendored files. `win_iconv.c` and `iconv.h` are byte-identical
to upstream `master` at commit `70a279dcfe6318bbe63e019e33b3230b55c19762`
(verified 2026-07-20 by hashing the raw files at that ref; this is later than
the v0.0.10 tag). Re-verify against these hashes when updating.

```
f974962d0f08addb74203e62e7e62ad28d81d3da270201411b0d9bbf642105f4  win_iconv.c
f0b087715d6a76ab844c7eb3a30187bd7f185dcbff72b5c97d73b5a576716f88  iconv.h
b6235815d8a19d3cbb48260ebc40da5a5c772a4b7af58401a2bc33e92348b08f  readme.txt
```
