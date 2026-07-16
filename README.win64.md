# libpst-win64 — Windows向け readpst.exe 自前ビルドフォーク

このリポジトリは [pst-format/libpst](https://github.com/pst-format/libpst) のフォークであり、
knowledge-baton プロジェクトの ADR-046 (PST展開readpstのWindows導入経路) に基づいて、
**Windows (x86_64) 向けの `readpst.exe` を自前でビルド・配布する**ことだけを目的とする。

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
- `LDFLAGS=-static` により可能な限り静的リンクし、単一の `readpst.exe` を目指す。
  CI 上で `objdump -p` により mingw64 系 DLL への動的依存が残っていないことを検証する
- configure オプション: `--enable-python=no` (Pythonバインディング不要)、
  `--enable-dii=no` (pst2dii = ImageMagick/libgd 依存機能は不要)。
  `--enable-libpst-shared` は既定 no のため、ツールは libpst に静的リンクされる

## 成果物の検証 (利用者向け)

GitHub Releases から zip をダウンロードしたら、必ず SHA256 を検証すること:

```powershell
# Windows PowerShell
Get-FileHash readpst.exe -Algorithm SHA256
# 表示されたハッシュが Release ページの SHA256SUMS と一致することを確認する
```

検証後、`readpst.exe` を PATH の通ったディレクトリへ配置し、knowledge-baton 側で
`kb doctor` を実行して `readpst: ok` になることを確認する。

## ライセンス

上流と同じ **GPLv2+** ([COPYING](COPYING) 参照)。Release に含まれる `readpst.exe` は
本リポジトリのソースからビルドされたものであり、対応するソースコードは本リポジトリで
公開されている (GPL §3 の要件を満たす)。

## 免責

- 実物の `.pst` ファイルでの動作検証は knowledge-baton プロジェクトの Deny リスト
  (実メール禁止) により実施していない。CI 上では `readpst -V` による起動確認までを行う
- 上流が新バージョンをリリースした場合は、本フォークで再ビルド・再検証が必要
