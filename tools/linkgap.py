"""Why the blocked runs do not link, measured rather than summarised.

    python tools/linkgap.py            what blocks each run, by cause
    python tools/linkgap.py --try      link them with externals supplied

`link.py` reports "needs an address for N symbol(s)" and stops there. Three
very different causes wear that one message, and they have three different
fixes -- so the number on its own says nothing about what to do next.

Each needed symbol is resolved from the retail word at its relocation site,
the way `loose_ends.py` does it, and classified by where it lands:

  CODE        a `.text` function start we have not matched. The only cause
              that needs more decompilation.
  DATA        `.rdata` or `.data` -- a float constant, a vtable, a string
              literal, a global. We have never written a data definition, so
              every one of these is missing by construction.
  NAME DRIFT  a `.text` address our manifest ALREADY matches, under a
              different invented name. Nothing is missing; one retail
              function was named twice. `build.py` reports these as calls
              and nothing acts on it.
  SELF        a symbol the run's own objects DEFINE. `describe()` builds its
              `have` set from the placed FUNCTION names, so a float constant
              or string literal the object emits into `.rdata` is invisible
              to it and counted as external.

WHAT `--try` ESTABLISHED. Supplying every external at its retail address, as
an ABSOLUTE, and linking the run at its retail address: 11 blocked runs
reproduce the image byte for byte, 1,244 bytes, with no new decompilation.
That is worth knowing and it is strictly weaker evidence than a run that
resolves everything internally -- the linker does the arithmetic, but the
address it works from was read out of the image. It must never be added to
the same number.

It also fails in two ways that are themselves the finding:

  * an ABSOLUTE satisfies an address SPLIT (ADDR32NB/SECREL) and cannot
    satisfy a CALL: link.exe answers a REL24 against an absolute with
    LNK2013 fixup overflow. 36 of 52 blocked runs need at least one call, so
    they are out of reach until the target is really placed.
  * a FLOAT LITERAL makes an isolated run unlinkable outright. The compiler
    emits `__real@...` into our own object; the linker resolves the symbol
    against that copy and places it wherever this small link puts `.rdata`,
    which is not retail's address, so the `lis`/`addi` pair comes out
    pointing somewhere else. Supplying an absolute instead collides with the
    object's own definition (LNK2005). Neither way works, and only a link
    that places `.rdata` AT ITS RETAIL ADDRESS can.

So the case for the whole-image link is not that it would be tidier. It is
that a large class of our matched code cannot be linked correctly any other
way, and that class is anything using a float constant.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import coffwrite
import link
import matched_table
from coffreloc import (COMPANION, WHOLE_WORD, functions_with_relocs,
                       solve_address, type_name)
from libmatch import coff_functions, pick_function
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent


def defined_symbols(blob):
    """Every symbol the object DEFINES, in any section -- not just .text."""
    if len(blob) < 20:
        return set()
    _m, _n, _t, psym, nsym, _o, _c = struct.unpack_from("<HHIIIHH", blob, 0)
    strtab = psym + nsym * 18
    out, i = set(), 0
    while i < nsym:
        o = psym + i * 18
        raw = blob[o:o + 8]
        if raw[:4] == bytes(4):
            off = struct.unpack_from("<I", blob, o + 4)[0]
            e = blob.find(bytes(1), strtab + off)
            name = blob[strtab + off:e].decode("latin1")
        else:
            name = raw.rstrip(bytes(1)).decode("latin1")
        _v, secnum, _ty, cls, naux = struct.unpack_from("<IhHBB", blob, o + 8)
        if secnum > 0 and cls == 2:
            out.add(name)
        i += 1 + naux
    return out


def resolve(img, run, needs):
    """symbol -> (address, {relocation type names}) from the retail word."""
    out, kinds = {}, {}
    for addr, _size, src, sym, flags in run:
        blob, _e = link.obj_for(src, flags)
        if blob is None:
            continue
        got, _w = pick_function(coff_functions(blob), sym)
        if got is None:
            continue
        for nm, _code, relocs in functions_with_relocs(blob):
            if nm != got[0]:
                continue
            pending = {}
            for r in sorted(relocs, key=lambda x: x.off):
                if r.sym not in needs or r.type in COMPANION:
                    continue
                kinds.setdefault(r.sym, set()).add(type_name(r.type))
                raw = img.read(addr + r.off, 4)
                if raw is None:
                    continue
                w = struct.unpack_from(">I", raw, 0)[0]
                if r.type in WHOLE_WORD:
                    out.setdefault(r.sym, w)
                    continue
                s = solve_address(r.type, w, addr + r.off)
                if not s:
                    continue
                if s[0] == "abs":
                    out.setdefault(r.sym, s[1])
                elif s[0] == "hi":
                    pending[r.sym] = s[1]
                elif s[0] == "lo" and r.sym in pending:
                    out.setdefault(r.sym,
                                   (pending[r.sym] + s[1]) & 0xFFFFFFFF)
    return out, kinds


def main(argv):
    img = Image()
    inv = dict(load_inventory())
    sized = link.compiled(matched_table.rows())
    runs = link.runs_of(sized)
    matched = set(a for a, _n, _s, _y, _f in sized)

    tally, runcause, blocked = {}, {}, 0
    tried = {"IDENTICAL": 0, "DIFFERS": 0, "FAILED": 0}
    gained = 0

    for run in runs:
        span, placed, needs, nofix, err = link.describe(run)
        if err or not needs:
            continue
        blocked += 1
        first = run[0][0]
        defined = set()
        for _a, _n, src, _s, flags in run:
            b, _e = link.obj_for(src, flags)
            if b is not None:
                defined |= defined_symbols(b)
        addrs, kinds = resolve(img, run, needs)

        causes = set()
        for s in sorted(needs):
            if s in defined:
                c = "SELF (the run's own object defines it)"
            else:
                a = addrs.get(s)
                if a is None:
                    c = "unresolved"
                elif (img.section_of(a) or "").startswith(".text"):
                    c = ("NAME DRIFT (we match that address)" if a in matched
                         else "CODE not matched" if a in inv
                         else "text, not a function start")
                else:
                    c = "DATA (%s)" % (img.section_of(a) or "?")
            tally[c] = tally.get(c, 0) + 1
            causes.add(c.split(" (")[0])
        key = " + ".join(sorted(causes))
        runcause[key] = runcause.get(key, 0) + 1

        if "--try" in argv:
            v, detail = attempt(img, run, needs, nofix, addrs, defined)
            tried[v] = tried.get(v, 0) + 1
            if v == "IDENTICAL":
                gained += span
            print("  %08X  %-10s %s" % (first, v, detail[:70]))

    total = sum(tally.values())
    print("")
    print("%d blocked run(s), %d symbol reference(s), by cause:\n"
          % (blocked, total))
    for k in sorted(tally, key=lambda k: -tally[k]):
        print("  %-40s %4d of %d" % (k, tally[k], total))
    print("")
    print("blocked runs by the SET of causes they carry:\n")
    for k in sorted(runcause, key=lambda k: -runcause[k]):
        print("  %-52s %3d" % (k[:52], runcause[k]))

    if "--try" in argv:
        print("")
        print("supplying every external at its retail address:")
        for k in sorted(tried, key=lambda k: -tried[k]):
            print("  %-10s %d" % (k, tried[k]))
        print("")
        print("%s byte(s) reproduce the image this way. Weaker evidence than"
              % "{:,}".format(gained))
        print("a self-contained run -- the arithmetic is the linker's, the")
        print("address is the image's -- so it is never added to that number.")
    return 0


def attempt(img, run, needs, nofix, addrs, defined):
    """Link the run with every external supplied as an ABSOLUTE."""
    first = run[0][0]
    if first % 8:
        return "FAILED", "run start not 8-aligned"
    missing = [s for s in needs if s not in addrs and s not in defined]
    if missing:
        return "FAILED", "%d external(s) unresolved" % len(missing)
    syms = dict((s, 0) for s in nofix if s not in defined)
    syms.update((s, a) for s, a in addrs.items() if s not in defined)
    tag = "gap%08X" % first
    (link.WORK / ("stubs_%s.obj" % tag)).write_bytes(coffwrite.absolutes(syms))
    objs = ["stubs_%s.obj" % tag]
    _sp, placed, _n, _nf, _e = link.describe(run)
    for _a, _n2, t in placed:
        if t + ".obj" not in objs:
            objs.append(t + ".obj")
    names = [n for _a, n, _t in placed]
    got, err = link._do_link(tag + "_probe", objs, names, link.BASE, 0,
                             placed[0][1])
    if got is None:
        return "FAILED", str(err)
    t = link._text_of(got[0])
    if t is None:
        return "FAILED", "probe has no .text"
    _ib, textrva, _p = t
    base = (first - textrva) & ~0xFFFF
    pad = first - (base + textrva)
    if pad < 0 or pad % 4:
        return "FAILED", "placement leaves %d" % pad
    got, err = link._do_link(tag, objs, names, base, pad, placed[0][1])
    if got is None:
        return "FAILED", str(err)
    pe, mapsyms = got
    t = link._text_of(pe)
    if t is None:
        return "FAILED", "no .text"
    imgbase, va, rawptr = t
    for addr, name, _t in placed:
        if mapsyms.get(name) != addr:
            return "FAILED", "%s misplaced" % name[:30]
    span = run[-1][0] + run[-1][1] - first
    off = rawptr + (first - imgbase) - va
    if off < 0 or off + span > len(pe):
        return "FAILED", "run lands outside the linked file"
    want = img.read(first, span)
    if pe[off:off + span] == want:
        return "IDENTICAL", "%d byte(s), %d external(s) supplied" % (
            span, len(syms))
    n = sum(1 for i in range(span // 4)
            if pe[off + i * 4:off + i * 4 + 4] != want[i * 4:i * 4 + 4])
    return "DIFFERS", "%d of %d word(s) differ" % (n, span // 4)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
