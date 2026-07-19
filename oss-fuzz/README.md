# OSS-Fuzz integration (registration-ready)

`projects/libpst/` is a complete [OSS-Fuzz](https://github.com/google/oss-fuzz)
project definition for continuous fuzzing of the libpst parser. It is provided
here so the setup is ready to submit; registering it is the repository owner's
decision.

## Contents

* `project.yaml` — engines (libFuzzer/AFL++/honggfuzz), sanitizers
  (address/undefined), contact, and repo metadata.
* `Dockerfile` — clones this repo into the OSS-Fuzz base-builder and installs
  the build dependencies (zlib, libgsf, autotools).
* `build.sh` shared with ClusterFuzzLite lives at `.clusterfuzzlite/build.sh`
  and is copied into the image; it builds `fuzz_pst_open`, `fuzz_pst_walk` and
  `fuzz_lzfu`, each with the format dictionary and a seed corpus (adversarial
  regression inputs + the valid `dist-list.pst` fixture).

## To register

1. Fork [google/oss-fuzz](https://github.com/google/oss-fuzz).
2. Copy `oss-fuzz/projects/libpst/` to `projects/libpst/` in that fork.
3. Test locally:
   ```sh
   python infra/helper.py build_image libpst
   python infra/helper.py build_fuzzers --sanitizer address libpst
   python infra/helper.py check_build libpst
   python infra/helper.py run_fuzzer libpst fuzz_pst_walk
   ```
4. Open a PR to google/oss-fuzz.

The same harnesses also run today, per-PR, via `.github/workflows/security.yml`
(short) and on a schedule via `.github/workflows/cflite_batch.yml`
(ClusterFuzzLite), so continuous fuzzing does not depend on OSS-Fuzz being
accepted.
