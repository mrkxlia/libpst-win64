# Test fixtures

## dist-list.pst

A small, valid Unicode (64-bit) PST file used to exercise real folder-walking,
message/contact/calendar parsing, fuzzing seeds, and coverage measurement.
libpst has no PST *writer*, and real mailboxes are intentionally never
committed, so a pre-existing public sample is the only practical way to get a
genuinely valid PST into the test suite.

* **Provenance:** `src/test/resources/dist-list.pst` from
  [java-libpst](https://github.com/rjohnsondev/java-libpst)
  (`raw.githubusercontent.com/rjohnsondev/java-libpst/master/src/test/resources/dist-list.pst`).
* **Contents:** 12 standard folders plus one contact
  (`contact1@rjohnson.id.au`) and one calendar entry — no private data.
* **Format:** magic `!BDN`, index type `0x17` (Unicode 64-bit), 271,360 bytes.
* **License:** java-libpst is dual-licensed Apache-2.0 / LGPL-2.1-or-later,
  both compatible with libpst's GPL-2.0-or-later. This file is committed
  solely as opaque test *data*; no java-libpst source code is linked or
  redistributed, and libpst's own license is unchanged.

This fixture is what `tests/test_smoke.py::test_full_walk_with_real_pst`
walks by default, what seeds the fuzzers under `fuzz/corpus/`, and what the
coverage job in `.github/workflows/security.yml` measures reachability
against.
