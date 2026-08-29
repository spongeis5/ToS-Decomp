"""Generate and verify the image's single-expression accessors.

    python tools/gen_accessors.py            write sources, verify, show rows
    python tools/gen_accessors.py --write    also append the rows

A census of every .text function of 24 bytes or less (4,092 of them) shows
the shapes and their populations. After the 346 constant returns that
gen_typeids.py handles, the next veins are accessors: field getters, field
setters, pointer adjusts, bitfield reads. Each is one expression, so each
can be written from its own encoding with no interpretation at all.

They are worth having for two reasons and not for a third. They are real
functions that a link needs; and each one PINS A FIELD OFFSET, which is
structure -- `lwz r3,0x24(r3)` says the class has a 4-byte field at 0x24,
and tools/vtables.py can say which class, because it knows which vtable the
function sits in. They are NOT worth having as a headline number: 500
four-word accessors are not comparable to 500 real functions, and the
reporting says so rather than letting the count speak for itself.

TWO SHAPES ARE DELIBERATELY REFUSED, and the reasons generalise.

  `lis rX,hi ; addi r3,rX,lo ; blr` -- returns the address of a global.
  Both halves carry relocations, so the only word left to compare is the
  `blr`. That is not a verified match, it is a match of one word that every
  function in the image ends with. 65 of these exist and none is generated.

  `stw r11,N(r3) ; blr` where r11 was never written. A function cannot read
  a register no one set, so the row is not a function -- it is the tail of a
  larger one, caught by a false start. Every generated shape is checked for
  self-consistency: it may read only parameter registers (r3-r10) and must
  write its result to r3 or f1.

Everything generated is then compiled and compared against the image, and a
row is emitted only for an exact match of the non-relocated words. A shape
that turns out to be wrong therefore costs nothing but a line of output.
"""

import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import coff_functions, trim_padding
from match import can_shrink, can_extend

import ppcdis
import xdkcc

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
MANIFEST = SRC / "manifest.txt"
PER_FILE = 40
FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]
PARAM = set(range(3, 11))          # r3..r10 arrive as arguments

BLR = 0x4E800020


def s16(x):
    return x - 0x10000 if x >= 0x8000 else x


def decode(ws):
    """-> dict describing a shape we are willing to generate, or None.

    Returns None for anything not on the list. Being conservative here is
    free: an unrecognised shape is simply left for a human, whereas a shape
    recognised WRONGLY produces a source that compiles, matches nothing, and
    has to be found again later.

    `preg`/`vreg` are the REGISTERS the pointer and the stored value arrive
    in, and they are the whole reason this returns registers rather than
    finished C. 17 of the first 362 candidates failed because the pointer was
    in r4 or r6 rather than r3 -- `addi r3,r4,232` is the second argument,
    not the first -- and a one-parameter signature can never produce that.
    The register fixes the argument position: r3 is argument 0, r4 argument
    1, and so on, so the signature is padded with unused leading parameters
    until the operand lands where the image puts it.
    """
    if ws[-1] != BLR:
        return None
    body = ws[:-1]

    if len(body) == 0:
        return {"kind": "empty"}

    if len(body) != 1:
        return None
    w = body[0]
    op = w >> 26
    d, a, off = (w >> 21) & 31, (w >> 16) & 31, s16(w & 0xFFFF)

    # Loads returning into r3.
    for lop, ctype in ((32, "u32"), (34, "u8"), (40, "u16")):
        if op == lop and d == 3 and a in PARAM and off >= 0:
            return {"kind": "get", "ctype": ctype, "off": off, "preg": a}
    # lfs f1,off(rA)
    if op == 48 and d == 1 and a in PARAM and off >= 0:
        return {"kind": "getf", "off": off, "preg": a}
    # addi r3,rA,off -- pointer adjust
    if op == 14 and d == 3 and a in PARAM:
        return {"kind": "adj", "off": off, "preg": a}
    # Stores. rS must ALSO be a parameter: a function cannot read a register
    # no one set, so `stw r11,N(r3)` is the tail of a larger function caught
    # by a false start, not a setter.
    for sop, ctype in ((36, "u32"), (38, "u8"), (44, "u16")):
        if op == sop and a in PARAM and d in PARAM and d != a and off >= 0:
            return {"kind": "set", "ctype": ctype, "off": off,
                    "preg": a, "vreg": d}
    return None


def emit(addr, d):
    """The C for one accessor, with arguments placed by register."""
    nm = "Acc_%08X" % addr
    if d["kind"] == "empty":
        return "void %s() { }\n" % nm

    slots = {}
    slots[d["preg"] - 3] = ("void*", "p")
    if "vreg" in d:
        slots[d["vreg"] - 3] = (d["ctype"], "v")
    args = []
    for i in range(max(slots) + 1):
        if i in slots:
            args.append("%s %s" % slots[i])
        else:
            args.append("int unused%d" % i)      # occupies rN, never read
    sigtxt = ", ".join(args)

    if d["kind"] == "get":
        return ("%s %s(%s) { return *(%s*)((char*)p + %d); }\n"
                % (d["ctype"], nm, sigtxt, d["ctype"], d["off"]))
    if d["kind"] == "getf":
        return ("float %s(%s) { return *(float*)((char*)p + %d); }\n"
                % (nm, sigtxt, d["off"]))
    if d["kind"] == "adj":
        return ("void* %s(%s) { return (char*)p + %d; }\n"
                % (nm, sigtxt, d["off"]))
    return ("void %s(%s) { *(%s*)((char*)p + %d) = v; }\n"
            % (nm, sigtxt, d["ctype"], d["off"]))


def already_sourced():
    done = set()
    for nm in ("manifest.txt", "attempts.txt"):
        p = SRC / nm
        if not p.exists():
            continue
        for line in p.read_text().splitlines():
            line = line.split("#")[0].strip()
            if not line:
                continue
            f = line.split()
            if len(f) >= 2:
                try:
                    done.add(int(f[1], 16))
                except ValueError:
                    pass
    return done


HEADER = '''#include "types.h"

// Single-expression accessors, %d of them in this file.
//
// Generated by tools/gen_accessors.py from each function's own encoding --
// a getter, a setter, a pointer adjust or an empty body, each one
// expression long. Every one was compiled and compared against the retail
// bytes before its manifest row was written; nothing here is asserted.
//
// The value is the FIELD OFFSETS. `lwz r3,0x24(r3)` says its class has a
// 4-byte field at 0x24, and tools/vtables.py can say which class, because
// it knows which vtable the function sits in.
//
// Do not hand-edit: regenerate.

'''


def main(argv):
    img = Image()
    inv = load_inventory()
    sizes = dict(inv)
    done = already_sourced()
    text = next(s for s in img.sections if s["name"] == ".text")
    tlo = text["va"]
    thi = text["va"] + (text["vsize"] or text["rawsz"])

    cands = []
    kinds = Counter()
    walked = 0
    for addr, size in sorted(inv):
        if not (tlo <= addr < thi) or size == 0 or size > 16:
            continue
        walked += 1
        if addr in done:
            continue
        raw = img.read(addr, size)
        if raw is None or len(raw) != size:
            continue
        ws = struct.unpack(">%dI" % (size // 4), raw)
        d = decode(ws)
        if d is None:
            continue
        kinds[d["kind"]] += 1
        cands.append((addr, d))

    print("%d .text function(s) of 16 bytes or less walked; %d already"
          % (walked, walked - len([1 for a, _s in inv
                                   if tlo <= a < thi and 0 < _s <= 16
                                   and a not in done])))
    print("sourced; %d match a shape this tool will generate:" % len(cands))
    for k, n in kinds.most_common():
        print("    %-6s %4d" % (k, n))
    if not cands:
        return 0

    groups = [cands[i:i + PER_FILE] for i in range(0, len(cands), PER_FILE)]
    rows, ok, bad = [], 0, []

    for gi, g in enumerate(groups):
        path = SRC / ("vt_acc_%02d.cpp" % (gi + 1))
        body = [HEADER % len(g)]
        for addr, d in g:
            body.append(emit(addr, d))
        path.write_text("".join(body), encoding="utf-8")

        blob, err = xdkcc.compile_obj(path, ROOT / "build/acc"
                                      / (path.stem + ".obj"), FLAGS,
                                      ROOT / "build/acc")
        if blob is None:
            print("")
            print("%s WOULD NOT COMPILE:" % path.name)
            print(err)
            return 1
        fns = {n: (c, m) for n, c, m in coff_functions(blob)}

        for addr, _d in g:
            want = "Acc_%08X" % addr
            picked = [n for n in fns if want in n]
            if len(picked) != 1:
                bad.append((addr, "symbol selects %d" % len(picked)))
                continue
            code, mask = trim_padding(*fns[picked[0]])
            tsize = sizes[addr]
            tb = img.read(addr, tsize)
            if tb is None:
                bad.append((addr, "unreadable"))
                continue
            grown = can_extend(img, sizes, code, mask, addr, tsize)
            if grown is not None:
                tb, tsize = grown, len(code)
            elif can_shrink(code, mask, tb, addr, tsize):
                tb, tsize = tb[:len(code)], len(code)
            if len(code) != tsize:
                bad.append((addr, "size %d vs %d" % (len(code), tsize)))
                continue
            diff = 0
            cmpd = 0
            for i in range(len(code) // 4):
                if not all(mask[i * 4:i * 4 + 4]):
                    continue
                cmpd += 1
                if (struct.unpack_from(">I", tb, i * 4)[0]
                        != struct.unpack_from(">I", code, i * 4)[0]):
                    diff += 1
            if diff or cmpd == 0:
                bad.append((addr, "%d of %d word(s) differ" % (diff, cmpd)))
                continue
            ok += 1
            rows.append("%-32s %08X  Acc_%08X"
                        % ("src/" + path.name, addr, addr))

    hits, misses = xdkcc.cache_stats()
    print("")
    print("%d file(s); cl invoked %d time(s), memo served %d"
          % (len(groups), misses, hits))
    print("%d of %d verified byte-identical; %d did not and get NO row"
          % (ok, len(cands), len(bad)))
    if bad:
        why = Counter(b.split("(")[0].strip() for _a, b in bad)
        for w, n in why.most_common(8):
            print("    %-40s %d" % (w, n))
        print("    the addresses, so a shape rule can be fixed rather than")
        print("    guessed at:")
        for a, w in bad:
            rows_ = ppcdis.image_range(a, sizes[a] // 4)
            print("      %08X  %-26s | %s"
                  % (a, w, " ; ".join(r[2].strip() for r in rows_)))

    if "--write" in argv and rows:
        with MANIFEST.open("a", encoding="utf-8") as f:
            f.write("\n".join(rows) + "\n")
        print("")
        print("appended %d row(s)" % len(rows))
    elif "--write" not in argv:
        print("")
        print("nothing written; pass --write")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
