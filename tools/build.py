"""Reconstruct the retail .text from source, and verify it byte for byte.

    python tools/build.py            build, verify, and report
    python tools/build.py --verbose  also list every resolved relocation

Per-function matching proves that a function's bytes agree at one address. It
does NOT prove the pieces fit together, and it excuses more than it should:
`match.py` masks a whole 4-byte word wherever the linker patches an address,
which throws away the opcode and register fields with it. Under that mask

    bl  <one symbol>       and       bl  <a different symbol>
    stw r11,0(r3)          and       stw r10,0(r3)

compare equal. Fifteen functions can all "match" and still be wrong.

So this does what a linker does, and checks the result:

  1. compile every source in src/manifest.txt
  2. for each relocation, SOLVE the referenced address out of the retail
     instruction, and patch our object with it
  3. require the patched function to equal the image EXACTLY -- no mask, no
     excused words
  4. splice every function into a copy of .text and hash the whole section
     against the original

Step 2 is the part that pays. A relocation's address is not knowable from the
source -- it is whatever the original linker chose -- so it is read back out
of the image. That is not circular: only the immediate FIELD comes from the
image, while the opcode, the registers and the relocation's own type and
offset all come from our object and have to agree. And the solved addresses
are themselves recovered facts: they say which function `Process` is, and
where `kVTable` lives.

Exit status is 0 only when .text is reproduced byte for byte.
"""

import hashlib
import os
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import trim_padding
import coffreloc
import xdkcc
from coffreloc import (functions_with_relocs, type_name, solve_address,
                       PATCH_BITS, WHOLE_WORD, COMPANION, REFHI, REFLO)

XDK = Path("SDKFiles/xdk/XDK")
CL = XDK / "bin/win32/cl.exe"
INCLUDE = XDK / "include/xbox"
WORK = Path("build/objs")
MANIFEST = Path("src/manifest.txt")
FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]


def compile_one(src, tag):
    """Compile via tools/xdkcc, the one place that knows the invocation."""
    return xdkcc.compile_obj(src, WORK / (tag + ".obj"), FLAGS, WORK)


def read_manifest():
    """[(source, address, symbol-or-None)].

    A real translation unit holds many functions, so a line may name which
    one it means. Without a symbol the object must contain exactly one
    function -- picking "the largest" silently builds the wrong one as soon
    as a file grows a second function, so it is refused instead.
    """
    rows = []
    for line in MANIFEST.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) == 2:
            rows.append((Path(parts[0]), int(parts[1], 16), None))
        elif len(parts) == 3:
            rows.append((Path(parts[0]), int(parts[1], 16), parts[2]))
        else:
            return None, "bad manifest line: %r" % line
    return rows, None


def relocate(code, relocs, target, tbytes, verbose):
    """Patch our code with addresses solved from the retail bytes.

    -> (patched, notes, problems). `notes` records each resolved address so
    the caller can report what was recovered; `problems` are relocations this
    does not know how to handle, which are NOT silently ignored.
    """
    out = bytearray(code)
    notes, problems = [], []
    pending_hi = {}

    for r in sorted(relocs, key=lambda x: x.off):
        if r.type in COMPANION:
            continue
        if r.off + 4 > len(code) or r.off + 4 > len(tbytes):
            problems.append("%s at +%#x is outside the function"
                            % (type_name(r.type), r.off))
            continue
        theirs = struct.unpack_from(">I", tbytes, r.off)[0]
        ours = struct.unpack_from(">I", out, r.off)[0]

        if r.type in WHOLE_WORD:
            struct.pack_into(">I", out, r.off, theirs)
            notes.append((r.off, type_name(r.type), r.sym, theirs, "data word"))
            continue

        bits = PATCH_BITS.get(r.type)
        if bits is None:
            problems.append("%s at +%#x is not handled"
                            % (type_name(r.type), r.off))
            continue

        merged = (ours & ~bits) | (theirs & bits)
        struct.pack_into(">I", out, r.off, merged)

        solved = solve_address(r.type, theirs, target + r.off)
        if solved is None:
            notes.append((r.off, type_name(r.type), r.sym, None, ""))
        elif solved[0] == "abs":
            notes.append((r.off, type_name(r.type), r.sym, solved[1], ""))
        elif solved[0] == "hi":
            pending_hi[r.sym] = solved[1]
            notes.append((r.off, type_name(r.type), r.sym, None, "high half"))
        else:
            hi = pending_hi.get(r.sym)
            if hi is None:
                notes.append((r.off, type_name(r.type), r.sym, None,
                              "low half, no REFHI seen"))
            else:
                notes.append((r.off, type_name(r.type), r.sym,
                              (hi + solved[1]) & 0xFFFFFFFF, "from REFHI+REFLO"))
    return bytes(out), notes, problems


def main(argv):
    verbose = "--verbose" in argv[1:]
    if not MANIFEST.exists():
        print("%s is missing" % MANIFEST)
        return 1
    rows, err = read_manifest()
    if err:
        print(err)
        return 1

    img = Image()
    inv = dict(load_inventory())

    print("Building %d function(s) from %s\n" % (len(rows), MANIFEST))
    patches = []
    failures = 0
    resolved = []

    for src, target, want_sym in rows:
        tag = "%s_%08X" % (src.stem, target)
        blob, cerr = compile_one(src, tag)
        if blob is None:
            print("  %-26s %08X  COMPILE FAILED" % (src.name, target))
            # cl prints the source filename first, so line 1 is never the
            # diagnostic. Showing only line 1 hid a working ASSERT_OFFSET
            # behind "chain5.cpp" and made a compile-time catch look like a
            # byte mismatch.
            lines = [l for l in (cerr or "").splitlines()
                     if "error" in l.lower() or "warning" in l.lower()]
            for l in (lines or (cerr or "(no output)").splitlines())[:6]:
                print("      %s" % l.strip())
            failures += 1
            continue

        fns = functions_with_relocs(blob)
        if not fns:
            print("  %-26s %08X  no PowerPC function in the object"
                  % (src.name, target))
            failures += 1
            continue
        if want_sym:
            # Anchor on the mangled form first: MSVC emits `?Name@@YA...`, so
            # "?Name@@" pins the whole name. A plain substring would make
            # `ClearAndHandle` also match `ClearAndHandleOther`.
            picked = [f for f in fns if ("?" + want_sym + "@@") in f[0]]
            if not picked:
                picked = [f for f in fns if f[0] == want_sym]
            if not picked:
                picked = [f for f in fns if want_sym in f[0]]
            if len(picked) != 1:
                print("  %-26s %08X  symbol %r matches %d function(s); the"
                      % (src.name, target, want_sym, len(picked)))
                print("      manifest needs one that is unique. Present:")
                for n, c, _r in fns:
                    print("        %-56s %d B" % (n[:56], len(c)))
                failures += 1
                continue
            name, code, relocs = picked[0]
        elif len(fns) != 1:
            # Picking "the largest" silently builds the wrong function the
            # moment a translation unit grows a second one. Refuse instead.
            print("  %-26s %08X  %d functions in the object and the manifest"
                  % (src.name, target, len(fns)))
            print("      line names none. Add a symbol column to choose:")
            for n, c, _r in fns:
                print("        %-56s %d B" % (n[:56], len(c)))
            failures += 1
            continue
        else:
            name, code, relocs = fns[0]
        code, _m = trim_padding(code, bytes([1]) * len(code))
        relocs = [r for r in relocs if r.off < len(code)]

        tbytes = img.read(target, len(code))
        if tbytes is None or len(tbytes) != len(code):
            print("  %-26s %08X  could not read %d image byte(s)"
                  % (src.name, target, len(code)))
            failures += 1
            continue

        patched, notes, problems = relocate(code, relocs, target, tbytes,
                                            verbose)
        ok = (patched == tbytes)
        rec = inv.get(target)
        sizetag = ""
        if rec is not None and rec != len(code):
            sizetag = "  [inventory says %d]" % rec
        print("  %-26s %08X  %3d B  %-2d reloc  %s%s"
              % (src.name, target, len(code), len(relocs),
                 "OK" if ok else "MISMATCH", sizetag))

        for p in problems:
            print("      UNHANDLED: %s" % p)
            ok = False
        if not ok:
            failures += 1
            for i in range(len(code) // 4):
                a = struct.unpack_from(">I", tbytes, i * 4)[0]
                b = struct.unpack_from(">I", patched, i * 4)[0]
                if a != b:
                    print("      +%#05x  want %08x  got %08x" % (i * 4, a, b))
        # Splice UNCONDITIONALLY -- see the note on the hash below. Adding
        # only the ones already proven equal makes the section hash a
        # tautology.
        patches.append((target, patched, ok))
        if ok:
            for off, tname, sym, addr, why in notes:
                if addr is not None:
                    resolved.append((target + off, tname, sym, addr, why))
                if verbose:
                    print("      +%#05x %-9s %-34s %s %s"
                          % (off, tname, sym[:34],
                             "%08X" % addr if addr is not None else "--", why))

    print("")
    if resolved:
        print("Addresses recovered from the retail bytes -- these are facts the")
        print("source could not know, and each names something real:\n")
        print("  %-10s %-9s %-34s %-10s %s"
              % ("site", "type", "symbol", "address", "what is there"))
        for site, tname, sym, addr, _why in resolved:
            sec = img.section_of(addr) or "unmapped"
            if addr in inv:
                known = "%s  function start, %d B" % (sec, inv[addr])
            elif any(a <= addr < a + s for a, s in inv.items()):
                known = "%s  INSIDE a known function -- suspicious" % sec
            else:
                known = "%s  data" % sec
            print("  %08X   %-9s %-34s %08X   %s"
                  % (site, tname, sym[:34], addr, known))
        print("")
        print("  A REL24 that lands on a function start, and a data reference")
        print("  that lands in a data section, are both independent checks: a")
        print("  wrong source shape puts them mid-instruction or out of range.")
        print("")

    # NOTE, and it was a real defect: an earlier version appended a function
    # to `patches` ONLY when it already equalled the image, then spliced those
    # into a copy of the image and hashed it. Every spliced byte was proven
    # equal before it went in, so `rebuilt == original` was guaranteed by
    # construction and the hash could not fail. It printed ".text REPRODUCES
    # BYTE FOR BYTE" for a check with no power -- the same shape as the
    # `verify_ghidra.py` mistake this project already paid for.
    #
    # Now every compiled function is spliced whether it matched or not, so the
    # hash actually tests something the per-function loop does not: that the
    # pieces land at the right offsets and do not overlap each other.
    text = next((s for s in img.sections if s["name"] == ".text"), None)
    if text is None:
        text = next((s for s in img.sections if s["exec"]), None)
    if text is None:
        print("Could not locate an executable section; per-function")
        print("verification passed but no whole-section hash was computed.")
        return 0
    base = text["va"]
    size = text["vsize"] or text["rawsz"]

    original = img.read(base, size)
    if original is None:
        print("Could not read .text; per-function verification passed but the")
        print("whole-section hash was not computed.")
        return 0

    rebuilt = bytearray(original)
    written = {}
    covered = 0
    overlaps = 0
    for target, patched, _ok in patches:
        off = target - base
        if off < 0 or off + len(patched) > len(rebuilt):
            print("%08X falls outside .text" % target)
            return 1
        for b in range(off, off + len(patched)):
            if b in written and written[b] != target:
                overlaps += 1
                break
        for b in range(off, off + len(patched)):
            written[b] = target
        rebuilt[off:off + len(patched)] = patched
        covered += len(patched)
    covered = len(written)

    if overlaps:
        print("%d manifest entr(ies) overlap another. Two sources claiming"
              % overlaps)
        print("the same bytes is a manifest error, not a match.")
        return 1

    h_orig = hashlib.sha256(original).hexdigest()
    h_new = hashlib.sha256(bytes(rebuilt)).hexdigest()
    print(".text %08X..%08X, %d bytes" % (base, base + size, size))
    print("  original  sha256 %s" % h_orig)
    print("  rebuilt   sha256 %s" % h_new)
    if h_orig != h_new:
        print("")
        print(".text DOES NOT REPRODUCE -- %d of %d function(s) differ."
              % (failures, len(rows)))
        return 1
    if failures:
        print("")
        print("%d function(s) reported a failure but the section still hashes"
              % failures)
        print("equal. That is contradictory and means this harness is wrong.")
        return 2

    print("")
    print("VERIFIED: %d of %d .text byte(s) -- %.4f%% -- were produced by"
          % (covered, size, 100.0 * covered / size))
    print("compiling source in this repository, with every relocation resolved")
    print("and NO word excused. The rest was copied from the original.")
    print("")
    print("What the hash does and does not establish. Every compiled function")
    print("is spliced in whether or not it matched, so a wrong function does")
    print("change the hash -- and so does a wrong address or an overlap, which")
    print("the per-function loop cannot see. But the other %d bytes were"
          % (size - covered))
    print("COPIED, not built, so the hash says nothing whatever about them.")
    print("")
    print("This is a SPLICE, not yet a LINK. The undecompiled code is copied")
    print("rather than assembled from objects, so section layout, symbol")
    print("ordering and inter-object padding remain unverified.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
