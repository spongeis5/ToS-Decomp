"""Generate and verify sources for the constant-returning virtuals.

    python tools/gen_typeids.py            write sources, verify, print rows
    python tools/gen_typeids.py --write    also append the rows to the manifest

194 functions in the image have a body of exactly `lis r3,hi / ori r3,r3,lo /
blr` -- a virtual returning a 32-bit constant. `tools/vtables.py` shows what
they are: 192 of the 194 appear in a vtable and only 2 are ever reached by a
direct `bl`, so they are virtual-only, and the constant is this engine's
substitute for RTTI (the game's own translation units were built with RTTI
off; zero of the 194 are RTTI-named, against 1,297 functions that are).

They are worth generating rather than hand-writing because they are
identical in shape and differ only in a constant, and worth generating
rather than handing to an agent because a script cannot mistype one of 194
hex literals and a reader cannot check 194 by eye.

HONEST ACCOUNTING. These are real functions in the retail image and a link
needs every one, but they are 12 bytes of the 8,467,964 in .text and they
must not be allowed to flatter the headline count. `tools/matched_table.py`
reports them as their own line, and the README states the total both ways.

The verification here compiles each generated file ONCE and checks every
function in it, using match.py's own `can_shrink`/`can_extend` rather than a
second implementation of the comparison -- three separate tools have now
disagreed with verify.py by reimplementing that, always in the direction
that gets believed.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import coff_functions, trim_padding
from match import can_shrink, can_extend

import xdkcc

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
MANIFEST = SRC / "manifest.txt"
PER_FILE = 32
FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]


def getters(img, inv):
    """Every function whose whole body is a constant return into r3.

    Three encodings, and all three are the same C:

        lis r3,hi ; ori r3,r3,lo ; blr    12 B, any 32-bit constant
        li  r3,imm ; blr                   8 B, constant in [-32768, 32767]
        lis r3,hi ; blr                    8 B, constant with a zero low half

    Restricted to r3 deliberately. A constant materialised into any other
    register is not a return value, and treating one as though it were would
    put a function in the manifest that says something the image does not.
    Measured: all 151 of the 8-byte li form and all 194 of the 12-byte form
    target r3, so the restriction excludes nothing here -- but it is the
    reason that is true rather than a coincidence being relied on.

    -> [(address, value, size)], value already sign-extended where the
    encoding is signed.
    """
    out = []
    for addr, size in sorted(inv):
        if size not in (8, 12):
            continue
        raw = img.read(addr, size)
        if raw is None or len(raw) != size:
            continue
        w = struct.unpack(">%dI" % (size // 4), raw)
        if w[-1] != 0x4E800020:                       # blr
            continue
        if size == 12:
            if (w[0] & 0xFC1F0000) != 0x3C000000:     # lis rD,hi
                continue
            if (w[1] & 0xFC000000) != 0x60000000:     # ori rA,rS,lo
                continue
            d, s, a = (w[0] >> 21) & 31, (w[1] >> 21) & 31, (w[1] >> 16) & 31
            if d == s == a == 3:
                out.append((addr, ((w[0] & 0xFFFF) << 16) | (w[1] & 0xFFFF),
                            size))
        else:
            d = (w[0] >> 21) & 31
            if d != 3:
                continue
            if (w[0] & 0xFC1F0000) == 0x38000000:     # li r3,imm (addi r3,0)
                imm = w[0] & 0xFFFF
                if imm >= 0x8000:
                    imm -= 0x10000
                out.append((addr, imm, size))
            elif (w[0] & 0xFC1F0000) == 0x3C000000:   # lis r3,hi
                out.append((addr, (w[0] & 0xFFFF) << 16, size))
    return out


def decl(addr, value, size):
    """The C for one stub. Signed where the encoding is, so the literal in
    the source reads the way the instruction does."""
    if size == 8 and value < 0:
        return "s32 TypeId_%08X() { return %d; }\n" % (addr, value)
    if size == 8 and value < 0x8000:
        return "s32 TypeId_%08X() { return %d; }\n" % (addr, value)
    return "u32 TypeId_%08X() { return 0x%08Xu; }\n" % (addr, value & 0xFFFFFFFF)


def already_sourced():
    done = set()
    for name in ("manifest.txt", "attempts.txt"):
        p = SRC / name
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

// Constant-returning virtuals, %d of them in this file.
//
// Each function's entire body materialises one constant into r3 and returns.
// 345 functions in the image do this, in two encodings: 194 as
//     lis r3,hi ; ori r3,r3,lo ; blr        any 32-bit constant
// and 151 as
//     li  r3,imm ; blr                      constant in [-32768, 32767]
// All 345 target r3, so all 345 are returns rather than some other use of a
// materialised constant.
//
// They are virtual-only: of the 12-byte form 192 of 194 appear in a vtable
// and 2 of 194 are reached by a direct bl; of the 8-byte form 140 of 151
// appear in a vtable and 11 of 151 are called directly. So the constant is
// an identity, not a computation. tools/vtables.py classifies the 12-byte
// population: 175 hash-shaped class IDs, 11 FourCC tags, 6 on a 0x100-spaced
// enum, and 2 COM HRESULTs (E_FAIL, E_NOTIMPL) that are not identities at
// all.
//
// The game's own code carries no RTTI -- zero of the 194 are RTTI-named,
// against 1,297 functions that are, all of them Havok's -- so this is the
// engine's own type system, and each constant names one class.
//
// Generated by tools/gen_typeids.py. Do not hand-edit: regenerate.

'''


def main(argv):
    img = Image()
    inv = load_inventory()
    all_g = getters(img, inv)
    done = already_sourced()
    todo = [(a, v, n) for a, v, n in all_g if a not in done]

    print("%d constant-returning virtual(s) in the image; %d already sourced;"
          % (len(all_g), len(all_g) - len(todo)))
    print("%d to generate" % len(todo))
    if not todo:
        return 0

    sizes = dict(inv)
    groups = [todo[i:i + PER_FILE] for i in range(0, len(todo), PER_FILE)]
    rows = []
    matched = 0
    failed = []

    for gi, g in enumerate(groups):
        path = SRC / ("vt_const_%02d.cpp" % (gi + 1))
        body = [HEADER % len(g)]
        for a, v, n in g:
            body.append(decl(a, v, n))
        path.write_text("".join(body), encoding="utf-8")

        blob, err = xdkcc.compile_obj(path, ROOT / "build/typeids"
                                      / (path.stem + ".obj"), FLAGS,
                                      ROOT / "build/typeids")
        if blob is None:
            print("")
            print("%s WOULD NOT COMPILE:" % path.name)
            print(err)
            return 1
        fns = {}
        for name, code, mask in coff_functions(blob):
            fns[name] = (code, mask)

        for a, v, _n in g:
            want = "TypeId_%08X" % a
            picked = [n for n in fns if want in n]
            if len(picked) != 1:
                failed.append((a, "symbol %s selects %d function(s), not 1"
                               % (want, len(picked))))
                continue
            code, mask = trim_padding(*fns[picked[0]])
            tsize = sizes.get(a)
            if tsize is None:
                failed.append((a, "not in the inventory"))
                continue
            tbytes = img.read(a, tsize)
            if tbytes is None:
                failed.append((a, "unreadable at %08X" % a))
                continue
            grown = can_extend(img, sizes, code, mask, a, tsize)
            if grown is not None:
                tbytes, tsize = grown, len(code)
            elif can_shrink(code, mask, tbytes, a, tsize):
                tbytes, tsize = tbytes[:len(code)], len(code)
            if len(code) != tsize:
                failed.append((a, "size %d vs %d" % (len(code), tsize)))
                continue
            bad = 0
            for i in range(len(code) // 4):
                if not all(mask[i * 4:i * 4 + 4]):
                    continue
                if (struct.unpack_from(">I", tbytes, i * 4)[0]
                        != struct.unpack_from(">I", code, i * 4)[0]):
                    bad += 1
            if bad:
                failed.append((a, "%d word(s) differ" % bad))
                continue
            matched += 1
            rows.append("%-32s %08X  TypeId_%08X"
                        % ("src/" + path.name, a, a))

    hits, misses = xdkcc.cache_stats()
    print("")
    print("%d file(s) written; cl invoked %d time(s), memo served %d"
          % (len(groups), misses, hits))
    print("%d of %d verified byte-identical" % (matched, len(todo)))
    if failed:
        print("%d did NOT match and are NOT in the rows below:" % len(failed))
        for a, why in failed:
            print("    %08X  %s" % (a, why))

    if "--write" in argv:
        if failed:
            print("")
            print("Refusing to append with %d failure(s) outstanding."
                  % len(failed))
            return 1
        with MANIFEST.open("a", encoding="utf-8") as f:
            f.write("\n".join(rows) + "\n")
        print("")
        print("appended %d row(s) to src/manifest.txt" % len(rows))
    else:
        print("")
        print("rows (not written; pass --write):")
        for r in rows[:5]:
            print("    " + r)
        print("    ... %d row(s) total" % len(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
