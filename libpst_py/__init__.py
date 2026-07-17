"""libpst-py — read Microsoft Outlook .pst files from Python.

A thin, self-contained binding over the libpst C core (compiled into the
extension module ``libpst_py._core``). The high-level API lets you open a
.pst file and walk its folder tree, reading each message's subject, sender,
date, plain-text body and attachment file names.

Example
-------
>>> import libpst_py
>>> pst = libpst_py.open("archive.pst")
>>> def walk(folder, indent=0):
...     print(" " * indent + folder.name)
...     for m in folder.messages:
...         print(" " * (indent + 2) + f"- {m.subject} ({m.sender})")
...     for sub in folder.subfolders:
...         walk(sub, indent + 2)
>>> walk(pst.root)
"""

from ._core import (  # noqa: F401
    Attachment,
    Folder,
    Message,
    PstFile,
    __version__,
    open,
)

__all__ = [
    "Attachment",
    "Folder",
    "Message",
    "PstFile",
    "open",
    "__version__",
]
