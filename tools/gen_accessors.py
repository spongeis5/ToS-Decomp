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
FLAGS_OS = ["/c", "/nologo", "/O2", "/Os", "/Gy", "/GS-", "/fp:fast"]

# Both levels, because the optimisation level is a property of the retail
# TRANSLATION UNIT and these functions come from all over the image. It shows
# up here exactly as MATCHED.md describes it: the virtual forwarders that
# fail at /O2 are the ones whose two loads share a register --
#
#     lwz r11,0(r3) ; lwz r11,108(r11) ; mtctr r11 ; bctr     /Os coalesced
#     lwz r11,0(r3) ; lwz r10,92(r11)  ; mtctr r10 ; bctr     /O2 fresh
#
# 56 of the first 110 failed on precisely that, 2 words each, and every one
# of them is the same source compiled one level down.
LEVELS = (("/O2", FLAGS), ("/O2 /Os", FLAGS_OS))
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
    # Virtual forwarders. 115 functions in the image tail-call through a
    # vtable slot, passing their arguments along untouched, in two shapes:
    #
    #     lwz rA,0(r3) ; lwz rB,N(rA) ; mtctr rB ; bctr
    #     lwz r3,M(r3) ; lwz rA,0(r3) ; lwz rB,N(rA) ; mtctr rB ; bctr
    #
    # The second dispatches on a MEMBER rather than on `this`. Neither
    # carries a relocation -- it is all register and displacement work -- so
    # unlike the `lis/addi/blr` family these are fully checkable.
    #
    # How many arguments the function takes does not matter and cannot be
    # recovered: arguments beyond r3 are never touched, so a one-parameter
    # forwarder emits the same bytes as a six-parameter one. The source here
    # therefore takes one, and asserts nothing about the rest.
    if len(ws) in (4, 5) and ws[-1] == 0x4E800420:          # bctr
        off = 0
        i = 0
        if len(ws) == 5:                                     # member first
            w0 = ws[0]
            if (w0 >> 26) != 32:
                return None
            d0, a0 = (w0 >> 21) & 31, (w0 >> 16) & 31
            if d0 != 3 or a0 != 3:
                return None
            off = s16(w0 & 0xFFFF)
            if off < 0:
                return None
            i = 1
        wv, wsl, wm = ws[i], ws[i + 1], ws[i + 2]
        if (wv >> 26) != 32 or (wsl >> 26) != 32:
            return None
        dv, av, ov = (wv >> 21) & 31, (wv >> 16) & 31, wv & 0xFFFF
        if av != 3 or ov != 0:                               # vptr at +0
            return None
        ds, as_, os_ = (wsl >> 21) & 31, (wsl >> 16) & 31, wsl & 0xFFFF
        if as_ != dv or (os_ & 3):
            return None
        if wm != (0x7C0903A6 | (ds << 21)):                  # mtctr rS
            return None
        return {"kind": "vfwd", "slot": os_ // 4, "off": off,
                "member": len(ws) == 5}

    # Everything below ends in `blr`. This guard used to sit at the top of
    # the function, which made the `bctr` branch above unreachable and the
    # whole virtual-forwarder family invisible -- the tool reported "0 match
    # a shape this tool will generate" and looked like it had simply found
    # nothing.
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
    if d["kind"] == "vfwd":
        if d["member"]:
            return ("void %s(void* p)\n"
                    "{\n"
                    "    VObj* q = *(VObj**)((char*)p + %d);\n"
                    "    q->vt->slot[%d](q);\n"
                    "}\n" % (nm, d["off"], d["slot"]))
        return ("void %s(VObj* p) { p->vt->slot[%d](p); }\n"
                % (nm, d["slot"]))

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

struct VObj;
typedef void (*VFn)(VObj*);
struct VTbl { VFn slot[256]; };
struct VObj { VTbl* vt; };

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
        if not (tlo <= addr < thi) or size == 0 or size > 20:
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

    print("%d .text function(s) of 20 bytes or less walked; %d already"
          % (walked, walked - len([1 for a, _s in inv
                                   if tlo <= a < thi and 0 < _s <= 20
                                   and a not in done])))
    print("sourced; %d match a shape this tool will generate:" % len(cands))
    for k, n in kinds.most_common():
        print("    %-6s %4d" % (k, n))
    if not cands:
        return 0

    groups = [cands[i:i + PER_FILE] for i in range(0, len(cands), PER_FILE)]
    rows, ok, bad = [], 0, []
    by_level = Counter()

    # Number AFTER whatever already exists. This restarted at 01 every run,
    # so a second pass -- adding the virtual forwarders to a src/ that
    # already held ten files of accessors -- overwrote vt_acc_01..03 and
    # silently unhoused the 120 functions whose manifest rows named them.
    # build.py caught it, as 120 functions reporting a failure while the
    # section still hashed equal; nothing in this tool would have.
    existing = sorted(SRC.glob("vt_acc_*.cpp"))
    first = 1
    for p in existing:
        try:
            first = max(first, int(p.stem.split("_")[-1]) + 1)
        except ValueError:
            pass

    for gi, g in enumerate(groups):
        path = SRC / ("vt_acc_%02d.cpp" % (first + gi))
        body = [HEADER % len(g)]
        for addr, d in g:
            body.append(emit(addr, d))
        path.write_text("".join(body), encoding="utf-8")

        objs = {}
        for lname, lflags in LEVELS:
            blob, err = xdkcc.compile_obj(
                path, ROOT / "build/acc" / ("%s_%s.obj"
                                            % (path.stem,
                                               lname.replace(" ", "").replace("/", ""))),
                lflags, ROOT / "build/acc")
            if blob is None:
                print("")
                print("%s WOULD NOT COMPILE at %s:" % (path.name, lname))
                print(err)
                return 1
            objs[lname] = {n: (c, m) for n, c, m in coff_functions(blob)}

        for addr, _d in g:
            want = "Acc_%08X" % addr
            best_err = None
            placed = False
            for lname, _lf in LEVELS:
                fns = objs[lname]
                picked = [n for n in fns if want in n]
                if len(picked) != 1:
                    best_err = best_err or "symbol selects %d" % len(picked)
                    continue
                code, mask = trim_padding(*fns[picked[0]])
                tsize = sizes[addr]
                tb = img.read(addr, tsize)
                if tb is None:
                    best_err = best_err or "unreadable"
                    continue
                grown = can_extend(img, sizes, code, mask, addr, tsize)
                if grown is not None:
                    tb, tsize = grown, len(code)
                elif can_shrink(code, mask, tb, addr, tsize):
                    tb, tsize = tb[:len(code)], len(code)
                if len(code) != tsize:
                    best_err = best_err or "size %d vs %d" % (len(code), tsize)
                    continue
                diff = cmpd = 0
                for i in range(len(code) // 4):
                    if not all(mask[i * 4:i * 4 + 4]):
                        continue
                    cmpd += 1
                    if (struct.unpack_from(">I", tb, i * 4)[0]
                            != struct.unpack_from(">I", code, i * 4)[0]):
                        diff += 1
                if diff or cmpd == 0:
                    best_err = best_err or ("%d of %d word(s) differ"
                                            % (diff, cmpd))
                    continue
                ok += 1
                by_level[lname] += 1
                if lname == "/O2":
                    rows.append("%-32s %08X  Acc_%08X"
                                % ("src/" + path.name, addr, addr))
                else:
                    rows.append("%-32s %08X  %-22s flags=%s"
                                % ("src/" + path.name, addr,
                                   "Acc_%08X" % addr,
                                   "/O2,/Os,/Gy,/GS-,/fp:fast"))
                placed = True
                break
            if not placed:
                bad.append((addr, best_err or "no level matched"))

    hits, misses = xdkcc.cache_stats()
    print("")
    print("%d file(s); cl invoked %d time(s), memo served %d"
          % (len(groups), misses, hits))
    for _ln, _lf in LEVELS:
        print("    %-8s %d" % (_ln, by_level[_ln]))
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
