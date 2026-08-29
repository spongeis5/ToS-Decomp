"""Disassembly backed by binutils, which knows Xenon VMX128.

Capstone cannot decode VMX128 at all -- 44,956 instructions across 55 forms
in this image, 2.12% of .text, concentrated in exactly the vector maths a game
engine is full of.  `build/ppcdis.exe` wraps binutils' PowerPC disassembler
with the PPC_OPCODE_VMX_128 dialect enabled.

Falls back to capstone only if the binary has not been built, and SAYS SO --
silently degrading to a decoder that drops 2% of instructions would make a
disassembly of a different function look like a disassembly of this one.
"""

import struct
import subprocess
import sys
import tempfile
from pathlib import Path

EXE = Path("build/ppcdis.exe")
IMAGE = Path("build/default.image.exe")
BASE = 0x82000000

_warned = [False]


def available():
    return EXE.exists()


def _fallback(words, start):
    if not _warned[0]:
        print("WARNING: build/ppcdis.exe is missing; falling back to capstone, "
              "which CANNOT decode VMX128. Instructions will be shown as .long.",
              file=sys.stderr)
        _warned[0] = True
    try:
        from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN
    except ImportError:
        return [(start + i * 4, w, "<no disassembler>") for i, w in enumerate(words)]
    md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
    out = []
    for i, w in enumerate(words):
        va = start + i * 4
        got = list(md.disasm(struct.pack(">I", w), va))
        if got:
            out.append((va, w, ("%s %s" % (got[0].mnemonic, got[0].op_str)).strip()))
        else:
            out.append((va, w, ".long 0x%08X   ; capstone has no VMX128" % w))
    return out


def _run(image, base, start, count):
    r = subprocess.run([str(EXE), str(image), "%X" % base, "%X" % start,
                        str(count)], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("ppcdis failed: %s" % r.stderr.strip())
    out = []
    for line in r.stdout.splitlines():
        p = line.split(None, 2)
        if len(p) < 3:
            continue
        out.append((int(p[0], 16), int(p[1], 16) if p[1] != "--------" else 0, p[2]))
    return out


def image_range(start, count, image=IMAGE, base=BASE):
    """[(va, word, text)] for `count` instructions from guest address start."""
    if not available():
        data = Path(image).read_bytes()
        off = start - base
        words = struct.unpack_from(">%dI" % count, data, off)
        return _fallback(words, start)
    return _run(image, base, start, count)


def words(seq, start=0):
    """Disassemble a bare sequence of instruction words.

    Used for object-file code, which is not in the image.  The words are
    written to a scratch file and handed to the same binutils backend, so
    object code and image code are read by ONE decoder rather than two that
    could disagree.
    """
    if not available():
        return _fallback(seq, start)
    blob = struct.pack(">%dI" % len(seq), *seq)
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        f.write(blob)
        tmp = f.name
    try:
        return _run(tmp, start, start, len(seq))
    finally:
        try:
            Path(tmp).unlink()
        except OSError:
            pass
