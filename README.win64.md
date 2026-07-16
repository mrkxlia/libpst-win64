# libpst-win64 — Windows向け readpst.exe ビルドフォーク

このリポジトリは [pst-format/libpst](https://github.com/pst-format/libpst) のフォークであり、
**Windows (x86_64) 上で PST ファイルを扱うための `readpst.exe` を自前でビルド・配布する**
ことを目的とする。

## 上流との関係

- 上流: [pst-format/libpst](https://github.com/pst-format/libpst) — Debian の libpst
  公式パッケージメンテナ (Paul Wise) が管理するリポジトリ。原作者 Carl Byington の
  公式配布 ([five-ten-sg.com/libpst](https://www.five-ten-sg.com/libpst/)) は 0.6.76
  (2021-03-27) を最後に休眠しており、こちらが事実上の維持されている上流である
- 本フォークはソースコードを**一切改変しない**。追加するのはビルド用の
  GitHub Actions ワークフロー (`.github/workflows/build-windows.yml`) と本READMEのみ
- 上流の更新は `git fetch upstream && git merge` で追従する

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

## ライセンス

上流と同じ **GPLv2+** ([COPYING](COPYING) 参照)。Release に含まれる `readpst.exe` は
本リポジトリのソースからビルドされたものであり、対応するソースコードは本リポジトリで
公開されている (GPL §3 の要件を満たす)。

## 免責

- CI 上では `readpst -V` による起動確認と静的リンク検証までを行う。実際の `.pst`
  ファイルでの動作は利用者側で確認すること
- 上流が新バージョンをリリースした場合は、本フォークで再ビルド・再検証が必要
