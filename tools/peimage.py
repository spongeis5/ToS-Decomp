"""Shared access to the unpacked retail image.

One derivation of "where does this guest address live in the file", used by
every tool, rather than each tool growing its own copy that can drift.
"""

import hashlib
import os
import struct
import sys
from pathlib import Path

IMAGE = Path("build/default.pe.exe")
FUNCTIONS = Path("build/functions.txt")

# The image this project's every number is about.
#
# Nothing pinned this until an audit asked what would happen if the image were
# regenerated from a different dump. The answer was: every address, every
# match and every count would silently be about a different program, and no
# tool would say so. A byte-matching decompilation is a claim about ONE image,
# so the image is named here and checked on load.
#
# If a legitimately different build is intended -- another region, a title
# update -- set TOS_ALLOW_IMAGE_MISMATCH=1 and update these, deliberately and
# in a commit of their own.
EXPECTED_SHA256 = (
    "c171aaad32708342fa59274467013fd1"
    "d0145c4974cd4225358aaf35d9e03614")
EXPECTED_SIZE = 11272192


class ImageMismatch(Exception):
    pass


def _verify(data, path):
    got = hashlib.sha256(data).hexdigest()
    if got == EXPECTED_SHA256 and len(data) == EXPECTED_SIZE:
        return
    msg = ("%s is not the image this project was built against.\n"
           "  expected  %d bytes, sha256 %s\n"
           "  got       %d bytes, sha256 %s\n"
           "Every address, match and count here is a claim about the expected\n"
           "image. Rebuild it with tools/xex.py from the retail DEFAULT.XEX,\n"
           "or set TOS_ALLOW_IMAGE_MISMATCH=1 if a different build is\n"
           "genuinely intended."
           % (path, EXPECTED_SIZE, EXPECTED_SHA256, len(data), got))
    if os.environ.get("TOS_ALLOW_IMAGE_MISMATCH"):
        print("WARNING: " + msg, file=sys.stderr)
        return
    raise ImageMismatch(msg)


class Image:
    def __init__(self, path=IMAGE, verify=True):
        self.data = Path(path).read_bytes()
        if verify and Path(path) == IMAGE:
            _verify(self.data, path)
        d = self.data
        o = struct.unpack_from("<I", d, 0x3C)[0]
        if d[o : o + 4] != b"PE\0\0":
            raise ValueError(f"{path}: not a PE image")
        self.nsec = struct.unpack_from("<H", d, o + 6)[0]
        optsz = struct.unpack_from("<H", d, o + 20)[0]
        self.base = struct.unpack_from("<I", d, o + 24 + 28)[0]
        sh = o + 24 + optsz
        self.sections = []
        for i in range(self.nsec):
            b = sh + i * 40
            name = d[b : b + 8].rstrip(b"\0").decode("latin1")
            vsize, va, rawsz, rawptr = struct.unpack_from("<IIII", d, b + 8)
            chars = struct.unpack_from("<I", d, b + 36)[0]
            self.sections.append(dict(
                name=name, va=self.base + va, vsize=vsize,
                rawsz=rawsz, rawptr=rawptr, chars=chars,
                exec=bool(chars & 0x20000000),
                initialized=rawptr != 0 and rawsz != 0))

    def offset(self, va):
        """File offset for a guest VA, or None if it is not backed.

        THE UNPACKED XEX IS A MEMORY IMAGE: RVA == offset in the buffer, and
        the PE header's PointerToRawData describes the ORIGINAL file layout,
        which no longer applies.  Using it reads the wrong bytes for every
        section from .text onward.

        The first three sections happen to have RVA == PointerToRawData,
        which is why the defect stayed invisible until .text.  It was settled
        by the entry point: at 822F8BC8 the RVA mapping gives
        `mflr r12 / bl / stwu r1,-0x1f0(r1)` -- a textbook prologue -- while
        the rawptr mapping gives a run of `lfs` loads that disassembles
        cleanly into nonsense.  A wrong mapping still produces instructions,
        which is exactly why this had to be decided by a control and not by
        whether the output looked like code.
        """
        rva = va - self.base
        if 0 <= rva < len(self.data):
            return rva
        return None

    def read(self, va, n):
        """n bytes at a guest VA, or None if the whole range is not backed."""
        o = self.offset(va)
        if o is None:
            return None
        if self.offset(va + n - 1) is None:
            return None
        return self.data[o : o + n]

    def section_of(self, va):
        for s in self.sections:
            if s["va"] <= va < s["va"] + (s["vsize"] or s["rawsz"]):
                return s["name"]
        return None


INVENTORY = Path("build/functions_all.txt")


def load_functions(path=FUNCTIONS):
    """The .pdata inventory as [(address, size)], sorted.

    This is the COMPILER'S table and misses leaf functions that need no unwind
    record. Prefer load_inventory() unless you specifically want .pdata.
    """
    p = Path(path)
    if not p.exists():
        raise FileNotFoundError(f"{path} -- run tools/pdata.py first")
    out = []
    for line in p.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        out.append((int(f[0], 16), int(f[1])))
    out.sort()
    return out


def load_inventory(path=INVENTORY):
    """The FULL function inventory as [(address, size)], sorted.

    .pdata (21,238) union Ghidra's analysis (25,737). The 4,499 functions
    Ghidra found and .pdata lacks are real -- mostly leaf functions with no
    unwind row -- and a tool keyed on .pdata alone silently refuses to work on
    them. Falls back to .pdata with a warning rather than pretending.
    """
    p = Path(path)
    if not p.exists():
        import sys
        print(f"WARNING: {path} missing; falling back to .pdata alone, which "
              f"is ~18% short. Run tools/inventory.py.", file=sys.stderr)
        return load_functions()
    out = []
    for line in p.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        out.append((int(f[0], 16), int(f[1])))
    out.sort()
    return out


# The large contiguous regions attribution identified as XDK library code.
# Candidate selection must exclude these: "not attributed" is NOT the same as
# "the title's own code", and the smallest unattributed leaves sit inside the
# big XDK block.
XDK_REGIONS = [
    (0x82100000, 0x821294A0),   # the D3D / XTL band
    (0x822F03E8, 0x82523A1C),   # one 2.31 MB XDK block
    (0x828A74A0, 0x82908510),   # the CRT
]


def in_xdk(va):
    return any(lo <= va < hi for lo, hi in XDK_REGIONS)
