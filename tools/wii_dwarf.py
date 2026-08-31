"""Read the Wii build's symbols and DWARF, and say how much is the same source.

    python tools/wii_dwarf.py             sections, tree, and the overlap
    python tools/wii_dwarf.py --tree      just the source tree
    python tools/wii_dwarf.py --names N   N function symbols, demangled-ish

The Wii release of this title shipped an UNSTRIPPED CodeWarrior ELF on the
disc, `DATA/files/SB09WiiMASTERWAD.elf`. It carries 4.68 MB of DWARF and
45,707 symbols where the 360 image carries none.

NOTHING HERE IS A MATCH, and nothing here may become one. Wii code is
CodeWarrior and 360 code is MSVC 15.00.8153 -- different register
allocation, inlining and scheduling, so not one byte transfers. What
transfers is what the source SAID: field layouts, function names and
signatures, and which file each function came from. `match.py` still owns
the only question that can put a row in the manifest.

WHY IT IS THE SAME SOURCE, which is the thing that had to be measured before
any of it was worth anything. `STT_FILE` holds bare basenames -- that is
what CodeWarrior emits and it says nothing either way. The real paths are in
`.debug_line`'s directory table, and 1,390 of them are rooted at
`C:/branches/SB09/main`, which is the branch the 360's own assert
registration stubs name (`tools/srcfiles.py`). Both builds compile
`NG/Source/Engine/...`; `NG` is not a 360-only directory.

The dump lives at `game_wii/` and is gitignored, exactly like `game/`. It is
not ours to distribute and no part of it is in this repository.
"""

import re
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ELF = ROOT / "game_wii/DATA/files/SB09WiiMASTERWAD.elf"
BS = chr(92)
BRANCH = "C:/branches/SB09"


def sections(d):
    """-> [(name, addr, offset, size, link, entsize)] for every section."""
    e_shoff, = struct.unpack_from(">I", d, 32)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(">HHH", d, 46)
    raw = [struct.unpack_from(">IIIIIIIIII", d, e_shoff + i * e_shentsize)
           for i in range(e_shnum)]
    stro = raw[e_shstrndx][4]

    def nm(x):
        return d[stro + x:d.index(b"\0", stro + x)].decode("latin1")

    return [(nm(s[0]), s[3], s[4], s[5], s[6], s[9]) for s in raw]


def symbols(d, sec):
    """-> [(name, value, size, type)] from .symtab, or [] if there is none."""
    s = next((x for x in sec if x[0] == ".symtab"), None)
    if s is None or not s[5]:
        return []
    _n, _a, off, size, link, entsz = s
    strtab = sec[link][2]
    out = []
    for k in range(size // entsz):
        o = off + k * entsz
        st_name, st_value, st_size, st_info = struct.unpack_from(">IIIB", d, o)
        if not st_name:
            continue
        e = d.index(b"\0", strtab + st_name)
        out.append((d[strtab + st_name:e].decode("latin1"),
                    st_value, st_size, st_info & 0xF))
    return out


def paths(d, sec):
    """-> Counter of directory strings under the shared branch.

    No DWARF parser: every NUL-terminated printable run in .debug_line and
    .debug_info is examined and the ones shaped like a path are kept. That
    OVER-collects rather than under-collects, which is the safe direction for
    a question about what exists.
    """
    pat = re.compile(rb"[ -~]{6,300}")
    dirs = Counter()
    for target in (".debug_line", ".debug_info"):
        s = next((x for x in sec if x[0] == target), None)
        if s is None:
            continue
        blob = d[s[2]:s[2] + s[3]]
        for m in pat.finditer(blob):
            x = m.group().decode("latin1").replace(BS, "/")
            if BRANCH not in x:
                continue
            x = x[x.index("C:/"):]
            if re.search(r"[.](cpp|c|h|hpp|inl)$", x, re.I):
                x = x.rsplit("/", 1)[0]
            dirs[x] += 1
    return dirs


def rtti_names():
    """-> the distinct class names the 360 recovered from its own RTTI."""
    out = set()
    for f in ("rtti_vtables.txt", "rtti_functions.txt", "vtables.txt"):
        p = ROOT / "build" / f
        if not p.exists():
            continue
        text = p.read_text(encoding="utf-8", errors="ignore")
        out.update(re.findall(r"[?.]AV([A-Za-z_][A-Za-z0-9_]*)@@", text))
    return sorted(out)


def main(argv):
    if not ELF.exists():
        print("%s is not there." % ELF.relative_to(ROOT).as_posix())
        print("")
        print("Put an extracted Wii disc at game_wii/ (it is gitignored, like")
        print("game/). REFUSING to print anything else: a report with no")
        print("input reads as 'nothing found' when it means 'nothing was")
        print("looked at'.")
        return 1

    d = ELF.read_bytes()
    if d[:4] != b"\x7fELF":
        print("%s is not an ELF." % ELF.name)
        return 1
    sec = sections(d)
    print("%s -- %s bytes, %d section(s)"
          % (ELF.name, "{:,}".format(len(d)), len(sec)))

    dwarf = [(n, sz) for n, _a, _o, sz, _l, _e in sec
             if n.startswith(".debug") or n == ".line"]
    if not dwarf:
        print("")
        print("NO DWARF SECTIONS. That is not the same as 'no symbols' --")
        print("check .symtab before concluding anything.")
    else:
        print("DWARF: %d section(s), %s bytes"
              % (len(dwarf), "{:,}".format(sum(s for _n, s in dwarf))))
        for n, sz in dwarf:
            print("   %-20s %s" % (n, "{:,}".format(sz)))

    syms = symbols(d, sec)
    kinds = Counter(t for _n, _v, _s, t in syms)
    TY = {0: "NOTYPE", 1: "OBJECT", 2: "FUNC", 3: "SECTION", 4: "FILE"}
    print("")
    print(".symtab: %d named symbol(s) -- %s"
          % (len(syms), ", ".join("%s=%d" % (TY.get(t, t), c)
                                  for t, c in sorted(kinds.items()))))

    if "--names" in argv:
        n = int(argv[argv.index("--names") + 1])
        print("")
        for nm_, val, sz, t in [x for x in syms if x[3] == 2][:n]:
            print("   %08X %6d  %s" % (val, sz, nm_))
        return 0

    dirs = paths(d, sec)
    print("")
    print("%d distinct directory string(s) under %s" % (len(dirs), BRANCH))
    if not dirs:
        print("NONE. The Wii build does not name that branch, so it is NOT")
        print("the tree the 360 names and the shared surface is unproven.")
        return 0
    tops = Counter()
    for k, v in dirs.items():
        parts = [p for p in k.split("/") if p]
        # Six components, not four: everything shares `C:/branches/SB09/main`,
        # so grouping any shallower collapses the whole tree into one row and
        # shows nothing.
        tops["/".join(parts[:6])] += v
    for k, v in sorted(tops.items(), key=lambda kv: -kv[1])[:14]:
        print("   %5d  %s" % (v, k))
    if "--tree" in argv:
        print("")
        for k in sorted(dirs):
            print("   %s" % k)
        return 0

    names = rtti_names()
    if not names:
        print("")
        print("build/rtti_*.txt is missing, so the overlap CANNOT be measured")
        print("here. Run tools/rtti.py. Not measuring it is not zero overlap.")
        return 0

    joined = "\n".join(n for n, _v, _s, _t in syms)
    dbg = next((x for x in sec if x[0] == ".debug_info"), None)
    if dbg is not None:
        joined += "\n" + d[dbg[2]:dbg[2] + dbg[3]].decode("latin1",
                                                          "ignore")
    hit = [n for n in names if n in joined]
    print("")
    print("%d of %d class name(s) the 360 recovered from RTTI appear in the"
          % (len(hit), len(names)))
    print("Wii build (%.1f%%). A name that does not appear is reported as"
          % (100.0 * len(hit) / len(names)))
    print("not appearing -- it is not evidence about anything else.")
    miss = [n for n in names if n not in joined]
    if miss:
        print("")
        print("absent from the Wii build (%d):" % len(miss))
        for n in miss[:20]:
            print("   %s" % n)
        if len(miss) > 20:
            print("   ... and %d more" % (len(miss) - 20))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
