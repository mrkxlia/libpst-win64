# libpst-win64

**A hardened fork of [pst-format/libpst](https://github.com/pst-format/libpst): a Windows `readpst.exe` build plus `libpst_py`, a Python library for reading Microsoft Outlook `.pst` files — with the parser fuzz-hardened along the way.**

*[pst-format/libpst](https://github.com/pst-format/libpst) のフォーク。Windows 版 `readpst.exe` と、`.pst` を読む Python ライブラリ `libpst_py` を提供し、パーサーを fuzzing で堅牢化しています。*

---

## What this fork is / このフォークについて

Upstream libpst is the de-facto maintained tree (the original five-ten-sg.com release has been dormant since 0.6.76, 2021-03-27). This fork started as "just a Windows build" but has grown three streams of change:

1. **Memory-safety hardening** — bugs found by fuzzing (libFuzzer + ASAN/UBSan) fixed across `src/` (heap OOB read/write, NULL derefs, uninitialized frees, signed-shift UB), plus path-traversal / MIME-header-injection hardening in `readpst.c`. See [`upstream-patches/`](upstream-patches/) — these are prepared for contribution back to upstream.
2. **Python library** — the legacy Boost.Python binding is replaced by a pybind11 module (`python/pybind_libpst.cpp`) distributed as the `libpst_py` wheel.
3. **Build / CI** — Windows `readpst.exe`, Linux `-Werror`, security checks (fuzz / ASAN / static analysis / coverage), and multi-platform wheels.

> 日本語: 上流は実質的な維持元です。本フォークは当初「Windows ビルドのみ」でしたが、現在は **(1) メモリ安全性修正**、**(2) Python ライブラリ化 (pybind11)**、**(3) ビルド/CI** の3系統に成長しています。メモリ安全性修正は上流還元用に [`upstream-patches/`](upstream-patches/) に整理済みです。

---

## `readpst.exe` (Windows CLI)

Download the `readpst-win64-*.zip` from the [Releases](https://github.com/mrkxlia/libpst-win64/releases) page, **verify the SHA256**, then unzip the whole archive (the `.exe` and its bundled DLLs must stay in the same directory):

```powershell
# Windows PowerShell
Get-FileHash readpst.exe -Algorithm SHA256
# Compare against SHA256SUMS in the zip / release notes (bundled DLL hashes are listed too)
.\readpst.exe -V          # prints the version
.\readpst.exe -o out archive.pst
```

The build uses MSYS2/MINGW64; full static linking is impossible because MSYS2's `libgsf-1.a` DLL-imports glib, so `libgcc`/`libstdc++` are linked statically and the remaining MinGW DLLs are bundled (their provenance is recorded in `DLLLIST.txt` inside the zip). CI verifies `readpst -V` runs from the bundled DLLs alone.

> 日本語: [Releases](https://github.com/mrkxlia/libpst-win64/releases) から zip を取得し、**SHA256 を検証**してから丸ごと展開してください（exe と同梱 DLL は同一ディレクトリに置く）。完全静的リンクは不可のため libgcc/libstdc++ のみ静的取り込み、残り DLL は同梱（由来は `DLLLIST.txt`）。

---

## `libpst_py` (Python library)

A self-contained pybind11 extension with the libpst C core compiled straight in — **no runtime dependency on a separate libpst shared library**. Wheels are built for **CPython 3.10–3.13** on **Linux (manylinux) / macOS (universal2) / Windows (AMD64)**.

### Install / インストール

**1. Prebuilt wheel — no compiler needed (recommended).** Download the wheel matching your Python and OS from the [Releases](https://github.com/mrkxlia/libpst-win64/releases) page and install it:

```bash
# example: Windows, CPython 3.12
pip install libpst_py-0.1.0-cp312-cp312-win_amd64.whl
```

**2. From source.** Works on all three OSes. Requirements:

- All: Python 3.10+, a C/C++ compiler, CMake, and network access at build time (the build fetches zlib via CMake `FetchContent`).
- **Windows:** Visual Studio 2019+ **Build Tools** (the "Desktop development with C++" workload). `iconv` is provided by the vendored, public-domain **win-iconv** in `third_party/win-iconv/` — nothing to install. zlib is fetched automatically.
- **Linux:** zlib development headers (`zlib1g-dev` / `zlib-devel`).
- **macOS:** zlib and iconv ship with the SDK.

```bash
pip install "git+https://github.com/mrkxlia/libpst-win64"
# or, from a clone:
pip install .
```

> 日本語:
> **(1) プリビルド wheel（コンパイラ不要・推奨）** — [Releases](https://github.com/mrkxlia/libpst-win64/releases) から自分の Python/OS に合う wheel を落として `pip install <wheel>`。対象は CPython 3.10–3.13、Linux/macOS(universal2)/**Windows(AMD64)**。
> **(2) ソースからビルド** — 全 OS で可能。**Windows は Visual Studio Build Tools (C++) + CMake + ネットワーク**が必要（iconv は同梱 win-iconv、zlib は自動取得）。`pip install "git+https://github.com/mrkxlia/libpst-win64"`。

### Usage / 使い方

```python
import libpst_py

pst = libpst_py.open("archive.pst")        # charset= optional, default UTF-8

def walk(folder):
    for m in folder.messages:
        # subject / sender / sender_name / to / cc / bcc / date / sent_date /
        # header / plain_text / html / rtf / importance / priority / message_id ...
        print(m.subject, "<", m.sender, ">")
        for a in m.attachments:
            # a.filename / a.mimetype / a.content_id / a.size / a.data (bytes)
            with open(a.filename, "wb") as fh:
                fh.write(a.data)
    for c in folder.contacts:
        print("contact:", c.fullname, c.email1, c.company_name)
    for ap in folder.appointments:
        print("appt:", ap.subject, ap.start, ap.end, ap.location)
    for j in folder.journals:
        print("journal:", j.subject, j.type)
    for sub in folder.subfolders:
        walk(sub)

walk(pst.root)
```

Type stubs (`_core.pyi` + `py.typed`) ship in the wheel, so editors and `mypy` see the full API.

> 日本語: `open()` で開き、`folder.messages / contacts / appointments / journals / subfolders` を走査します。添付本体は `a.data`（bytes）。型スタブ同梱なので mypy/エディタ補完が効きます。

---

## Security & fuzzing / セキュリティと fuzzing

- **Deterministic gate:** `.github/workflows/security.yml` replays a pinned corpus of adversarial reproducers **plus the valid PST fixture** under ASAN+UBSan on every PR — this is the hard memory-safety gate.
- **Exploratory fuzzing:** short libFuzzer runs on each PR (non-gating; new crashes are uploaded as artifacts) and a scheduled ClusterFuzzLite batch (`.github/workflows/cflite_batch.yml`). A registration-ready OSS-Fuzz project lives in [`oss-fuzz/`](oss-fuzz/).
- **Coverage:** a valid PST fixture (`tests/fixtures/dist-list.pst`) lifted core-parser region coverage from ~7.5% to ~35%, which is what surfaced the bugs fixed here.
- **Static analysis & supply chain:** advisory CodeQL (`.github/workflows/codeql.yml`) plus cppcheck/clang-tidy reports; GitHub Actions and the fetched zlib are pinned to commit SHAs, with Dependabot keeping the pins current.
- **Reporting:** see [`SECURITY.md`](SECURITY.md) for private vulnerability reporting and the security policy.

> 日本語: 確定ゲートは「固定再現コーパス + 有効 PST を ASAN/UBSan で再生」。PR 時の短時間 fuzz は非ゲート（クラッシュはアーティファクト化）、深掘りは ClusterFuzzLite の定期実行と、登録準備済みの OSS-Fuzz 設定（[`oss-fuzz/`](oss-fuzz/)）が担います。静的解析は CodeQL(参考情報) + cppcheck/clang-tidy。GitHub Actions と取得 zlib はコミット SHA 固定で Dependabot が追従。脆弱性の報告方法は [`SECURITY.md`](SECURITY.md) を参照してください。

---

## Upstream relationship / 上流との関係

Track upstream with `git fetch upstream && git merge`. The only likely conflicts are this fork's memory-safety patches, catalogued with their reproducers in [`upstream-patches/README.md`](upstream-patches/README.md) and ready to submit to pst-format/libpst.

The autotools build still works as upstream (`./configure && make`); `--enable-python` now defaults to **no** (the maintained binding is the pybind11 wheel above, built via CMake), so a plain `./configure` no longer needs Boost.

> 日本語: 上流追従は `git fetch upstream && git merge`。衝突しうるのは本フォークのメモリ安全性パッチのみ（[`upstream-patches/README.md`](upstream-patches/README.md)）。autotools ビルドは従来通り動作し、`--enable-python` の既定は **no**（Python は上記 pybind11 wheel が本流）。

---

## License / ライセンス

**GPL-2.0-or-later**, same as upstream (see [`COPYING`](COPYING)). The `readpst.exe` in each Release is built from this repository's source (satisfying GPL §3), and `libpst_py` is likewise GPLv2+.

> 日本語: 上流と同じ **GPLv2+**（[`COPYING`](COPYING)）。Release の `readpst.exe` は本リポジトリのソースからビルドされ、対応ソースは本リポジトリで公開（GPL §3 準拠）。`libpst_py` も GPLv2+。

## Disclaimer / 免責

CI verifies `readpst -V` startup and the deterministic sanitizer replay; validate behavior on your own `.pst` files. PyPI publishing is intentionally left to the user — wheels are distributed via GitHub Releases here, not PyPI.

> 日本語: CI は `readpst -V` 起動確認とサニタイザ再生までを検証します。実 `.pst` での動作は各自で確認してください。PyPI 公開は行わず、wheel は GitHub Releases で配布します。
