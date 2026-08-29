"""SUPERSEDED by tools/verify_mapping.py. Kept as a worked example of a
verification that could not fail.

This compares Ghidra's memory blocks against this project's own read of the
file. Both used `PointerToRawData`, so when that turned out to be the WRONG
mapping for a XEX (see FINDINGS.md section 8) the two agreed while both were
wrong, and this script reported "14 of 14 blocks agree" on a program whose
.text was entirely misplaced.

Two derivations of one fact are only a cross-check if they are INDEPENDENT.
These shared the defect, so this was one derivation run twice.

`verify_mapping.py` replaces it: it scores both candidate mappings against the
.pdata function extents, an oracle that takes no part in either, and asks a
question only a correct mapping can answer well (does a function END on a
return instruction?). That separated them 98.2% to 3.6%.

The original docstring follows.

---

Mark Ghidra's memory map against an independent read of the same file.

Ghidra reported "Import succeeded" for an image whose import directory it
parsed as noise and whose exception directory it refused outright.  That is
exactly the shape of a tool reporting a benign value, so the map it produced
is checked here rather than trusted.

Reads BLOCKS-BEGIN..BLOCKS-END from ReportBlocks.java's output on stdin (or
a file) and, for every initialised block, recomputes the same FNV-1a hash of
the first 256 bytes straight out of build/default.pe.exe using the section
table this project parsed itself.

    analyzeHeadless ... -postScript ReportBlocks > build/blocks.txt
    python tools/verify_ghidra.py build/blocks.txt
"""

import re
import struct
import sys
from pathlib import Path

IMAGE = Path("build/default.pe.exe")


def sections(data):
    o = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, o + 6)[0]
    optsz = struct.unpack_from("<H", data, o + 20)[0]
    base = struct.unpack_from("<I", data, o + 24 + 28)[0]
    sh = o + 24 + optsz
    out = []
    for i in range(nsec):
        b = sh + i * 40
        name = data[b : b + 8].rstrip(b"\0").decode("latin1")
        vsize, va, rawsz, rawptr = struct.unpack_from("<IIII", data, b + 8)
        chars = struct.unpack_from("<I", data, b + 36)[0]
        out.append(dict(name=name, vsize=vsize, va=base + va, rawsz=rawsz,
                        rawptr=rawptr, chars=chars))
    return base, out


def fnv1a(buf):
    h = 0xCBF29CE484222325
    for v in buf:
        h ^= v
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def main(argv):
    src = Path(argv[1]) if len(argv) > 1 else None
    text = src.read_text(errors="replace") if src else sys.stdin.read()

    lines = [l for l in text.splitlines() if l.startswith("BLOCK ")
             or l.startswith("image_base") or l.startswith("language")
             or l.startswith("function_count")]
    if not any(l.startswith("BLOCK ") for l in lines):
        print("no BLOCK lines found -- the script did not run, and this is "
              "NOT_MEASURED rather than a pass", file=sys.stderr)
        return 2

    data = IMAGE.read_bytes()
    _base, secs = sections(data)
    by_name = {s["name"]: s for s in secs}

    for l in lines:
        if not l.startswith("BLOCK "):
            print("  " + l)
    print()

    agree = disagree = uninit = unmatched = 0
    blocks = 0
    for l in lines:
        if not l.startswith("BLOCK "):
            continue
        blocks += 1
        parts = l.split()
        name, start, end, size, perms, ghash = parts[1], parts[2], parts[3], parts[4], parts[5], parts[6]
        if name == "Headers":
            # Ghidra's synthetic block for the PE headers, at file offset 0.
            # Not a section, so it has no row in our table; check it against
            # the file anyway rather than filing it as unmatched.
            s = dict(name=name, va=_base, rawptr=0, rawsz=int(size, 16),
                     vsize=int(size, 16))
        else:
            s = by_name.get(name)
        if s is None:
            print(f"  {name:<10} {start}  NO MATCHING SECTION in our own table")
            unmatched += 1
            continue
        if ghash == "uninitialized":
            print(f"  {name:<10} {start}  {perms}  uninitialized (bss) -- not checkable")
            uninit += 1
            continue
        # Hash exactly what Ghidra hashed: the first 256 bytes OF THE BLOCK,
        # which for a short section is shorter than 256.  Using a fixed 256
        # against a 12-byte section compares two different populations.
        n = min(256, int(size, 16))
        ours = fnv1a(data[s["rawptr"] : s["rawptr"] + n])
        ok = f"{ours:016x}" == ghash
        va_ok = int(start, 16) == s["va"]
        print(f"  {name:<10} {start}  {perms}  size {int(size,16):>9,}  "
              f"va {'ok' if va_ok else 'MISMATCH ours %08X' % s['va']}  "
              f"bytes {'agree' if ok else 'DISAGREE ours %016x' % ours}")
        if ok and va_ok:
            agree += 1
        else:
            disagree += 1

    print()
    print(f"{blocks} block(s) reported by Ghidra, {len(secs)} section(s) in our table")
    print(f"  {agree} agree, {disagree} disagree, {uninit} uninitialised, "
          f"{unmatched} unmatched")
    if disagree or unmatched:
        print("\nTHE MAP IS WRONG. Nothing read out of this program is a fact "
              "about the title.")
        return 1
    if agree == 0:
        print("\nNOTHING WAS CHECKED -- 0 blocks compared. This is NOT_MEASURED.")
        return 2
    print("\nThe memory map agrees with an independent read of the file.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
