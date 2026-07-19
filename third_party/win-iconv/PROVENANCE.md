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
