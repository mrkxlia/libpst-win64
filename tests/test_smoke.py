"""Smoke / integration tests for the libpst_py wheel.

The repository ships no valid .pst fixture (libpst has no PST writer and real
mailboxes are intentionally not committed), so these tests verify that:

  * the compiled extension imports and exposes the expected API, and
  * the parser handles non-PST / malformed input without crashing — either
    raising a clean Python exception or returning an empty tree.

The malformed inputs are the fuzzing reproducers pinned under
regression/fuzz-corpus/, which double as adversarial open() inputs here.
When a valid .pst fixture is available, set LIBPST_PY_TEST_PST to its path to
additionally exercise folder-walking and message reading.
"""

import os
import pathlib

import pytest

import libpst_py


def test_api_surface():
    assert hasattr(libpst_py, "open")
    assert hasattr(libpst_py, "PstFile")
    assert hasattr(libpst_py, "Folder")
    assert hasattr(libpst_py, "Message")
    assert hasattr(libpst_py, "Attachment")
    assert isinstance(libpst_py.__version__, str)


def test_open_nonexistent_raises(tmp_path):
    missing = tmp_path / "does-not-exist.pst"
    with pytest.raises(Exception):
        libpst_py.open(str(missing))


def test_open_empty_file_raises(tmp_path):
    empty = tmp_path / "empty.pst"
    empty.write_bytes(b"")
    with pytest.raises(Exception):
        libpst_py.open(str(empty))


def test_open_garbage_does_not_crash(tmp_path):
    garbage = tmp_path / "garbage.pst"
    garbage.write_bytes(b"not a pst file" * 100)
    # Either raises cleanly or yields an empty tree — never crashes the process.
    try:
        pst = libpst_py.open(str(garbage))
    except Exception:
        return
    assert pst.root is not None


def _repro_dir():
    here = pathlib.Path(__file__).resolve().parent
    return here.parent / "regression" / "fuzz-corpus"


@pytest.mark.parametrize("repro", sorted(_repro_dir().glob("*")) if _repro_dir().is_dir() else [])
def test_fuzz_reproducers_do_not_crash(repro):
    # These are adversarial inputs that previously crashed the C parser.
    # After the 2A fixes they must be handled without crashing the process.
    try:
        pst = libpst_py.open(str(repro))
    except Exception:
        return
    assert pst.root is not None


@pytest.mark.skipif(
    not os.environ.get("LIBPST_PY_TEST_PST"),
    reason="set LIBPST_PY_TEST_PST to a valid .pst to run the full walk test",
)
def test_full_walk_with_real_pst():
    path = os.environ["LIBPST_PY_TEST_PST"]
    pst = libpst_py.open(path)
    total = 0

    def walk(folder):
        nonlocal total
        total += len(folder.messages)
        for m in folder.messages:
            _ = (m.subject, m.sender, m.date, m.plain_text, m.attachment_names)
        for sub in folder.subfolders:
            walk(sub)

    walk(pst.root)
    assert total >= 0
