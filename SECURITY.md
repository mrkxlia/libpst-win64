# Security Policy / セキュリティポリシー

## Reporting a vulnerability / 脆弱性の報告

Please report suspected vulnerabilities **privately** via GitHub's private
vulnerability reporting:
<https://github.com/mrkxlia/libpst-win64/security/advisories/new>

Do **not** open a public issue for security reports. We aim to acknowledge
reports within 7 days. If the issue is in the upstream libpst parser rather
than this fork's additions, we will coordinate with
[pst-format/libpst](https://github.com/pst-format/libpst).

> 日本語: 脆弱性の疑いがある場合は、公開 Issue ではなく上記の GitHub
> プライベート脆弱性報告からご連絡ください。7日以内の一次応答を目標とします。
> 上流 libpst 由来の問題は上流と連携して対応します。

## Supported versions / サポート対象

Only the `main` branch and the latest tagged release receive security fixes.

> 日本語: セキュリティ修正の対象は `main` ブランチと最新のタグ付きリリースのみです。

## Scope / 対象範囲

- `readpst.exe` (Windows CLI) and the other CLI tools built from `src/`
  (`readpst`, `lspst`, `pst2ldif`, `nick2ldif`, `pst2dii`)
- The `libpst` C core (`src/`)
- The `libpst_py` Python wheel (`python/`, `libpst_py/`)

## Threat model & posture / 脅威モデルと体制

The primary attack surface is **parsing untrusted `.pst` files**. Hardening
this path is an explicit goal of the fork:

- A hard ASAN/UBSAN regression gate replays a pinned adversarial corpus on
  every relevant change (`.github/workflows/security.yml`, `asan-ubsan` job).
- Continuous fuzzing: in-repo libFuzzer harnesses (`fuzz/`) plus scheduled
  ClusterFuzzLite batch fuzzing (`.github/workflows/cflite_batch.yml`).
- Advisory static analysis: CodeQL (`.github/workflows/codeql.yml`),
  cppcheck and clang-tidy reports in `security.yml`.
- `-Werror` builds on Linux CI; GitHub Actions and the fetched zlib are
  pinned to commit SHAs; Dependabot watches both.
- Output paths derived from PST content are sanitized (`check_filename`)
  against directory traversal.
- Memory-safety fixes found by fuzzing are catalogued for upstream
  contribution in [`upstream-patches/`](upstream-patches/).

> 日本語: 主要な攻撃面は「信頼できない `.pst` ファイルの解析」です。
> 固定した敵対的コーパスの ASAN/UBSAN 再生ゲート、継続的 fuzzing
> (libFuzzer + ClusterFuzzLite)、CodeQL 等の静的解析、CI の SHA 固定と
> Dependabot、PST 由来パスのサニタイズ、上流還元用の修正カタログ
> (`upstream-patches/`) により堅牢化しています。
