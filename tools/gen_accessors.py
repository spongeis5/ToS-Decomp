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
# How far up the size range to walk. It was 20 while decode() only accepted a
# one-instruction body, where nothing larger could ever be recognised. The
# interpreter below reads a straight line of any length, so the cap is now
# only a bound on how long a body is worth trying. It is set from the
# measured population rather than chosen: above 100 bytes the census of
# relocation-free straight-line unmatched rows is empty, and between 44 and
# 100 it still holds 34 functions and 2,252 bytes -- field-copy and
# initialiser bodies, which are exactly what a straight line gets long doing.
WALK_MAX = 100
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


# ---------------------------------------------------------------------------
# Straight-line expression recovery.
#
# decode() above is a whitelist of shapes, and a body of exactly ONE
# instruction. A census of the 3,172 unmatched .text rows of 24 bytes or less
# puts 688 functions and 12,500 bytes in bodies that are straight-line,
# carry no relocation, and read only their own parameters -- across 254
# DISTINCT SHAPES. Whitelisting 254 shapes one at a time is the thing this
# project keeps learning not to do; the shapes are cheap to recover and
# expensive to guess at one by one.
#
# So the body is INTERPRETED instead: every register holds a symbolic value,
# each instruction transforms it, and at `blr` the value in r3 (or f1) is the
# return expression and the stores are the side effects, in order. Anything
# not understood returns None, which costs nothing -- the function is simply
# left for a human, exactly as an unrecognised shape already was.
#
# This decides NOTHING about correctness. Every generated source still goes
# through the same compile-and-compare below, using match.py's can_shrink and
# can_extend, and a source that does not reproduce the bytes gets no row.
# ---------------------------------------------------------------------------

CTY = {"u8": "unsigned char", "u16": "unsigned short", "u32": "unsigned int",
       "s8": "signed char", "s16": "short", "f32": "float", "f64": "double"}
DLOAD = {32: "u32", 34: "u8", 40: "u16", 42: "s16"}
DSTORE = {36: "u32", 38: "u8", 44: "u16"}
DFLOAT_L = {48: "f32", 50: "f64"}
DFLOAT_S = {52: "f32", 54: "f64"}
BINOP = {"add": "+", "sub": "-", "mul": "*", "and": "&", "or": "|",
         "xor": "^", "shl": "<<", "shr": ">>", "eq": "=="}


def _mask(mb, me):
    if mb <= me:
        return sum(1 << (31 - i) for i in range(mb, me + 1))
    return (sum(1 << (31 - i) for i in range(mb, 32))
            | sum(1 << (31 - i) for i in range(0, me + 1)))


def _k(v):
    return ("k", v & 0xFFFFFFFF)


def _rlwinm(x, sh, mb, me):
    """-> the symbolic value of ROTL32(x, sh) & MASK(mb, me), or None.

    Only the forms that ARE a shift or a masked shift in C are recovered. A
    genuine rotate has no C spelling MSVC would turn back into one word, so
    returning None for it is not a gap to fill later -- it is the honest
    answer.
    """
    m = _mask(mb, me)
    if sh == 0:
        return ("and", x, _k(m))
    if m == _mask(0, 31 - sh):
        return ("shl", x, _k(sh))
    n = 32 - sh
    if m == _mask(n, 31):
        return ("shr", x, _k(n))
    if m & ~(0xFFFFFFFF >> n) == 0:
        return ("and", ("shr", x, _k(n)), _k(m))
    return None


def sym_eval(ws):
    """-> {"kind": "expr", ...} for a straight-line body, or None.

    Refuses, in the same direction the rest of this tool refuses: an
    instruction it does not model, a register read before anything wrote it
    (a row that does that is the TAIL of a larger function caught by a false
    start, not a function), or a word that would carry a relocation.
    """
    if not ws or ws[-1] != BLR or len(ws) < 2:
        return None
    gpr = dict((r, ("p", r - 3)) for r in PARAM)
    fpr = dict((r, ("fp", r - 1)) for r in range(1, 9))
    stores = []
    used_ptr, used_int, used_flt = set(), set(), set()
    # The SCHEDULE, which is a fact about the encoding and not a guess. A
    # field copy appears in the image two ways:
    #
    #   lfs f0,8(r4) ; lfs f13,4(r4) ; lfs f12,0(r4)      loads BATCHED
    #   stfs f12,0(r3) ; stfs f13,4(r3) ; stfs f0,8(r3)
    #
    #   lwz r11,8(r4) ; stw r11,8(r3) ; lwz r11,4(r4) ... loads INTERLEAVED
    #
    # `*dst = *src;` three times running compiles to the second. Fourteen
    # bodies failed on exactly that difference and nothing else -- six of
    # nine words, every one of them a load or a store in the wrong place.
    # Reading the loads into locals first compiles to the first. Which
    # spelling to use is therefore not a preference: the instruction stream
    # says which one produced it.
    loads = []
    batched = [True]

    def note_load(v):
        if batched[0] and v not in loads:
            loads.append(v)
        return v

    def rd(r):
        v = gpr.get(r)
        if v is None:
            raise ValueError("reads r%d, which nothing set" % r)
        if v[0] == "p":
            used_int.add(v[1])
        return v

    def rdf(r):
        v = fpr.get(r)
        if v is None:
            raise ValueError("reads f%d, which nothing set" % r)
        if v[0] == "fp":
            used_flt.add(v[1])
        return v

    def base(r):
        v = rd(r)
        if v[0] == "p":
            used_ptr.add(v[1])
            used_int.discard(v[1])
        return v

    try:
        for w in ws[:-1]:
            op = w >> 26
            d, a, b = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
            imm = w & 0xFFFF
            simm = s16(imm)
            if op in DLOAD:
                if stores:
                    batched[0] = False
                gpr[d] = note_load(("ld", DLOAD[op], base(a), simm))
            elif op in DSTORE:
                stores.append(("st", DSTORE[op], base(a), simm, rd(d)))
            elif op in DFLOAT_L:
                if stores:
                    batched[0] = False
                fpr[d] = note_load(("ld", DFLOAT_L[op], base(a), simm))
            elif op in DFLOAT_S:
                stores.append(("st", DFLOAT_S[op], base(a), simm, rdf(d)))
            elif op == 14:                                   # addi / li
                gpr[d] = _k(simm) if a == 0 else ("add", rd(a), _k(simm))
            elif op == 7:                                    # mulli
                gpr[d] = ("mul", rd(a), _k(simm))
            elif op == 24:                                   # ori (and mr)
                gpr[a] = rd(d) if imm == 0 else ("or", rd(d), _k(imm))
            elif op == 26:                                   # xori
                gpr[a] = ("xor", rd(d), _k(imm))
            elif op == 28:                                   # andi.
                gpr[a] = ("and", rd(d), _k(imm))
            elif op == 21:                                   # rlwinm
                v = _rlwinm(rd(d), b, (w >> 6) & 31, (w >> 1) & 31)
                if v is None:
                    return None
                gpr[a] = v
            elif op == 31:
                xo = (w >> 1) & 0x3FF
                if xo == 266:                                # add
                    gpr[d] = ("add", rd(a), rd(b))
                elif xo == 40:                               # subf
                    gpr[d] = ("sub", rd(b), rd(a))
                elif xo == 444:                              # or / mr
                    gpr[a] = rd(d) if d == b else ("or", rd(d), rd(b))
                elif xo == 28:                               # and
                    gpr[a] = ("and", rd(d), rd(b))
                elif xo == 316:                              # xor
                    gpr[a] = ("xor", rd(d), rd(b))
                elif xo == 235 or xo == 75:                  # mullw / mulhw
                    if xo != 235:
                        return None
                    gpr[d] = ("mul", rd(a), rd(b))
                elif xo == 26:                               # cntlzw
                    gpr[a] = ("clz", rd(d))
                elif xo == 954:                              # extsb
                    gpr[a] = ("sx8", rd(d))
                elif xo == 922:                              # extsh
                    gpr[a] = ("sx16", rd(d))
                elif xo == 24:                               # slw
                    gpr[a] = ("shl", rd(d), rd(b))
                elif xo == 536:                              # srw
                    gpr[a] = ("shr", rd(d), rd(b))
                else:
                    return None
            else:
                return None
    except ValueError:
        return None

    ret = gpr.get(3)
    retf = fpr.get(1)
    kind, val = "void", None
    if ret is not None and ret != ("p", 0):
        kind, val = "int", ret
        used_int.update(_params(val, used_ptr))
    elif retf is not None and retf != ("fp", 0):
        kind, val = "float", retf
        used_flt.update(_params(val, used_ptr, True))
    elif not stores:
        return None                    # nothing computed, nothing stored
    if val is None and not stores:
        return None
    return {"kind": "expr", "ret": kind, "val": val, "stores": stores,
            "ptr": used_ptr, "int": used_int, "flt": used_flt,
            "loads": loads, "batched": batched[0]}


def _params(v, ptr, flt=False):
    """Parameter indices a value READS, so a signature can be built."""
    out = set()
    stack = [v]
    while stack:
        x = stack.pop()
        if not isinstance(x, tuple):
            continue
        if x[0] == ("fp" if flt else "p"):
            if x[1] not in ptr:
                out.add(x[1])
        for y in x[1:]:
            if isinstance(y, tuple):
                stack.append(y)
    return out


def _clz_eq(v):
    """`cntlzw` then a shift to bit 0 is `x == 0`; recognise it as that.

    The image spells `x == 1` as `addi -1 / cntlzw / rlwinm 27,31,31`, so the
    subtraction has to be folded back into the comparison or the generated C
    prints `(x + 0xFFFFFFFF) == 0` and the compiler has no reason to produce
    the same three words.
    """
    if not isinstance(v, tuple):
        return v
    if v[0] in ("shr", "and") and isinstance(v[1], tuple):
        inner = _clz_eq(v[1])
        if (v[0] == "shr" and isinstance(inner, tuple) and inner[0] == "clz"
                and v[2] == ("k", 5)):
            x = _clz_eq(inner[1])
            if isinstance(x, tuple) and x[0] == "add" and x[2][0] == "k":
                return ("eq", x[1], _k(-x[2][1]))
            return ("eq", x, _k(0))
        # `rlwinm 27,31,31` is a shift to bit 0 AND a one-bit mask, and the
        # mask is redundant once the shift has become `==`. Leaving it in
        # printed `(x == 2) & 1`, which is a different expression for the
        # compiler to think about than the one the image was written from.
        if (v[0] == "and" and isinstance(inner, tuple) and inner[0] == "eq"
                and v[2] == ("k", 1)):
            return inner
        return (v[0], inner) + tuple(v[2:])
    return tuple([v[0]] + [_clz_eq(y) if isinstance(y, tuple) else y
                           for y in v[1:]])


def vstr(v, tmp=None):
    """C for one symbolic value. Everything is parenthesised.

    `tmp` maps a value to a local that already holds it, so a batched body
    can name `t0` where the plain spelling would repeat the load.
    """
    if tmp and v in tmp:
        return tmp[v][0]
    k = v[0]
    if k == "p":
        return "a%d" % v[1]
    if k == "fp":
        return "f%d" % v[1]
    if k == "k":
        return "0x%Xu" % v[1]
    if k == "ld":
        b = vstr(v[2], tmp)
        return None if b is None else (
            "(*(%s*)((char*)%s + %d))" % (CTY[v[1]], b, v[3]))
    if k == "clz":
        return None                     # only meaningful inside _clz_eq
    if k == "sx8":
        return "((int)(signed char)%s)" % vstr(v[1], tmp)
    if k == "sx16":
        return "((int)(short)%s)" % vstr(v[1], tmp)
    if k in BINOP:
        l, r = vstr(v[1], tmp), vstr(v[2], tmp)
        if l is None or r is None:
            return None
        return "(%s %s %s)" % (l, BINOP[k], r)
    return None


def emit_expr(addr, d):
    """The C for one recovered straight-line body, or None if it will not
    print (a `cntlzw` that no shift turned into a comparison, say)."""
    nm = "Acc_%08X" % addr
    val = _clz_eq(d["val"]) if d["val"] is not None else None
    body = []

    # A batched body reads its sources into locals first, in the order the
    # image loads them, so the compiler has the same values live at the same
    # time. See the note in sym_eval: this is read off the schedule, not
    # chosen -- an interleaved body still gets the plain spelling, which is
    # the one that reproduces it.
    tmp = {}
    if d.get("batched") and len(d["stores"]) >= 2 and len(d["loads"]) >= 2:
        for i, lv in enumerate(d["loads"]):
            t = vstr(lv)
            if t is None:
                return None
            tmp[lv] = ("t%d" % i, lv[1])
        for lv in d["loads"]:
            nm_, cty = tmp[lv]
            body.append("    %s %s = %s;"
                        % (CTY[cty], nm_,
                           vstr(lv, dict((k, v) for k, v in tmp.items()
                                         if k is not lv))))

    for _t, cty, bs, off, v in d["stores"]:
        vt = vstr(_clz_eq(v), tmp)
        bt = vstr(bs, tmp)
        if vt is None or bt is None:
            return None
        body.append("    *(%s*)((char*)%s + %d) = (%s)%s;"
                    % (CTY[cty], bt, off, CTY[cty], vt))
    rt = "void"
    if val is not None:
        vt = vstr(val, tmp)
        if vt is None:
            return None
        rt = "float" if d["ret"] == "float" else "unsigned int"
        body.append("    return (%s)%s;" % (rt, vt))

    slots = {}
    for i in d["ptr"]:
        # `char*`, not `void*`. A pointer parameter is not only a load base:
        # `lwz r11,0(r3) ; mulli r11,r11,9936 ; add r11,r11,r3` computes an
        # element address, and the recovered value prints as `(... + a0)`,
        # which is arithmetic on the parameter itself. Seven bodies in one
        # generated file failed with C2036 "'void *' : unknown size" for
        # exactly that. `char*` makes the arithmetic legal and byte-sized,
        # which is what the instruction does; it does not change how the
        # argument is passed.
        slots[i] = ("char*", "a%d" % i)
    for i in d["int"]:
        slots.setdefault(i, ("unsigned int", "a%d" % i))
    args = []
    if slots:
        for i in range(max(slots) + 1):
            args.append("%s %s" % slots[i] if i in slots
                        else "int unused%d" % i)
    for i in sorted(d["flt"]):
        args.append("float f%d" % i)
    if not args:
        args = ["void"]
    return "%s %s(%s)\n{\n%s\n}\n" % (rt, nm, ", ".join(args),
                                      "\n".join(body))


def emit(addr, d):
    """The C for one accessor, with arguments placed by register."""
    nm = "Acc_%08X" % addr
    if d["kind"] == "expr":
        return emit_expr(addr, d)
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
    unprintable = 0
    for addr, size in sorted(inv):
        if not (tlo <= addr < thi) or size == 0 or size > WALK_MAX:
            continue
        walked += 1
        if addr in done:
            continue
        raw = img.read(addr, size)
        if raw is None or len(raw) != size:
            continue
        ws = struct.unpack(">%dI" % (size // 4), raw)
        # The whitelist first, unchanged: it is the tested one, and where the
        # two agree its C is the shorter spelling. sym_eval only sees what it
        # refuses.
        d = decode(ws) or sym_eval(ws)
        if d is None:
            continue
        if emit(addr, d) is None:
            unprintable += 1
            continue
        kinds[d["kind"]] += 1
        cands.append((addr, d))

    print("%d .text function(s) of %d bytes or less walked; %d already"
          % (walked, WALK_MAX,
             walked - len([1 for a, _s in inv
                           if tlo <= a < thi and 0 < _s <= WALK_MAX
                           and a not in done])))
    print("sourced; %d match a shape this tool will generate:" % len(cands))
    for k, n in kinds.most_common():
        print("    %-6s %4d" % (k, n))
    if unprintable:
        print("    %d body was recovered but has no C spelling that would"
              % unprintable)
        print("    produce the same words (a bare rotate, a cntlzw no shift")
        print("    turned into a comparison); those get no source at all.")
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
