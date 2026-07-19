# libpst-win64 — Windows向け readpst.exe ビルドフォーク

このリポジトリは [pst-format/libpst](https://github.com/pst-format/libpst) のフォークであり、
**Windows (x86_64) 上で PST ファイルを扱うための `readpst.exe` を自前でビルド・配布する**
ことを目的とする。

## 上流との関係

- 上流: [pst-format/libpst](https://github.com/pst-format/libpst) — Debian の libpst
  公式パッケージメンテナ (Paul Wise) が管理するリポジトリ。原作者 Carl Byington の
  公式配布 ([five-ten-sg.com/libpst](https://www.five-ten-sg.com/libpst/)) は 0.6.76
  (2021-03-27) を最後に休眠しており、こちらが事実上の維持されている上流である
- **フォーク独自の変更方針**: 当初は「ソース無改変 + ビルド CI 追加のみ」だったが、
  現在はセキュリティ強化のため上流ソース (`src/`) にもパッチを保持している。
  変更は大きく以下の3系統:
  1. **メモリ安全性の修正** — fuzzing (libFuzzer + ASAN/UBSAN) で発見した
     ヒープ境界外読み書き・NULL 参照・未初期化解放・整数シフト UB 等を修正
     (`src/libpst.c`, `src/lzfu.c`, `src/vbuf.c`, `src/timeconv.c`,
     `src/readpst.c`, `src/define.h`)。パス・トラバーサル / MIME ヘッダ
     インジェクション対策 (`readpst.c`) も含む
  2. **Python ライブラリ化** — Boost.Python 実装を pybind11 (`python/pybind_libpst.cpp`)
     へ置き換え、`libpst_py` として wheel 配布 (`CMakeLists.txt` / `pyproject.toml`)
  3. **ビルド/CI** — Windows `readpst.exe` ビルド、Linux `-Werror` ビルド、
     セキュリティ検査 (fuzz/ASAN/静的解析/カバレッジ)、wheel ビルド
- これらメモリ安全性修正は上流 (pst-format/libpst) へ還元予定であり、
  再現コーパスと合わせて `upstream-patches/` に整理している
- 上流の更新は `git fetch upstream && git merge` で追従する。上流と衝突しうる
  のは上記パッチのみで、その一覧は `upstream-patches/README.md` を参照

## ビルド方式

- GitHub Actions の `windows-latest` ランナー + [MSYS2](https://www.msys2.org/)
  (`msys2/setup-msys2@v2`) の MINGW64 環境でビルドする
- 依存ライブラリ (libgsf / glib2 / libiconv / zlib / TRE(regex) / winpthread) は
  MSYS2 の公式パッケージで解決する。**ビルドの起点となる libpst ソースは本リポジトリの
  ものだけを使う** (他者のビルド済み libpst バイナリは使わない)
- **リンク方式は「最小限の DLL 同梱」**: MSYS2 配布の `libgsf-1.a` は glib を
  DLL インポートする前提でビルドされており、glib2/libgsf をソースから再ビルド
  しない限り完全静的リンクは不可能なため、単一 exe 化は断念した。代わりに
  libgcc/libstdc++ のみ静的に取り込み、必要な mingw64 DLL を zip に同梱する。
  同梱 DLL の由来 (MSYS2 パッケージ名・バージョン) は zip 内の `DLLLIST.txt` に
  記録され、CI 上で「MSYS2 の PATH に依存せず同梱 DLL だけで `readpst -V` が
  動くこと」を検証している
- configure オプション: `--enable-python=no` (Pythonバインディング不要)、
  `--enable-dii=no` (pst2dii = ImageMagick/libgd 依存機能は不要)。
  `--enable-libpst-shared` は既定 no のため、ツールは libpst に静的リンクされる

## 成果物の検証 (利用者向け)

GitHub Releases から zip をダウンロードしたら、必ず SHA256 を検証すること:

```powershell
# Windows PowerShell
Get-FileHash readpst.exe -Algorithm SHA256
# 表示されたハッシュが zip 内 (および Release ノート) の SHA256SUMS と一致することを確認する
# 同梱 DLL のハッシュも SHA256SUMS に含まれる
```

検証後、zip を任意のディレクトリへ**丸ごと**展開し (exe と同梱 DLL は同じ
ディレクトリに置く必要がある)、`readpst -V` でバージョンが表示されることを
確認する。同梱 DLL の由来は `DLLLIST.txt` を参照。

## Python ライブラリ (libpst-py)

`readpst.exe` とは別に、**pybind11 ベースの Python バインディング** (`libpst-py`) を
同梱している。libpst の C コアを拡張モジュールに静的に取り込んだ単一モジュールで、
実行時に libpst の共有ライブラリへ依存しない。旧 Boost.Python 実装
(`python/python-libpst.cpp`) を置き換える新実装 (`python/pybind_libpst.cpp`)。

### API (最小)

```python
import libpst_py

pst = libpst_py.open("archive.pst")        # charset= も指定可 (既定 UTF-8)
def walk(folder, indent=0):
    print(" " * indent + folder.name)
    for m in folder.messages:
        # m.subject / m.sender / m.sender_name / m.date (ISO) /
        # m.plain_text / m.attachment_names / m.attachments[].filename,mimetype,size
        print(" " * (indent + 2) + f"- {m.subject} <{m.sender}>")
    for sub in folder.subfolders:
        walk(sub, indent + 2)
walk(pst.root)
```

### ビルド対象プラットフォーム

`cibuildwheel` (`.github/workflows/wheels.yml`) で **Linux (manylinux) / macOS** 向け
wheel を CPython 3.10〜3.13 でビルドする。**Windows (win_amd64) は対象外**:
libpst コアは iconv を必須とするが MSVC/Windows の libc は iconv を提供しないため。
Windows で PST を扱う場合は本リポジトリの `readpst.exe` (上記) を使う。

### PyPI 公開手順 (このリポジトリでは未実施)

公開は PyPI アカウント・トークンの設定が必要なため利用者側で行う。準備は整っている:

1. PyPI でパッケージ名 `libpst-py` の空きを確認 (使用済みなら `pyproject.toml` の
   `name` を変更)。
2. PyPI の Trusted Publishing (OIDC) を設定するか、API トークンを GitHub の
   リポジトリ Secrets に `PYPI_API_TOKEN` として登録する。
3. `wheels.yml` の成果物 (各 OS の `wheels-*` と `sdist`) をダウンロードし、
   `twine upload` するか、`pypa/gh-action-pypi-publish` を使う publish ジョブを
   タグ push 時に走らせる (このリポジトリには未追加 — 公開判断は利用者に委ねる)。
4. ローカル確認: `python -m build` → `pip install dist/*.whl` →
   `pytest tests`。

## ライセンス

上流と同じ **GPLv2+** ([COPYING](COPYING) 参照)。Release に含まれる `readpst.exe` は
本リポジトリのソースからビルドされたものであり、対応するソースコードは本リポジトリで
公開されている (GPL §3 の要件を満たす)。同梱の Python バインディング (`libpst-py`) も
同じく GPLv2+。

## 免責

- CI 上では `readpst -V` による起動確認と静的リンク検証までを行う。実際の `.pst`
  ファイルでの動作は利用者側で確認すること
- 上流が新バージョンをリリースした場合は、本フォークで再ビルド・再検証が必要
