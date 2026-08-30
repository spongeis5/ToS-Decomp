"""Verify every source in src/ that no manifest row mentions.

    python tools/sweep.py            report
    python tools/sweep.py --write    record what it finds

Work gets stranded. A parallel agent writes a source, compiles it, sees it
match, and is killed by a rate limit before it appends the manifest row --
seven were, in one batch, leaving 44 source files and 7 rows. The sources are
the expensive part and they are all still there; only the bookkeeping is
missing, and the bookkeeping is the part a machine can redo.

So this reads the TARGET ADDRESS out of the source itself. Every source in
this project opens with a `// sub_XXXXXXXX` comment naming what it decompiles
-- that convention is now load-bearing, not decorative.

Nothing here trusts a claim. A file is compiled and compared at both
optimisation levels, and only an exact match of the non-relocated words gets
a manifest row; anything else is reported with its score and, with --write,
recorded in attempts.txt where the near-misses live. An agent's own report
that a function matched is not evidence -- three times this session a tool
that decides "does this match?" disagreed with verify.py -- so the comparison
here is match.py's own can_shrink/can_extend, not a second implementation.
"""

import re
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
ATTEMPTS = SRC / "attempts.txt"
WORK = ROOT / "build/sweep"

# A source may declare that it can have no row, by carrying this marker and
# its reason. Opting out is then a deliberate act recorded beside the
# evidence, rather than a file quietly absent from both lists -- which is
# how three of them accumulated at once without anything noticing.
NO_ROW_MARKER = "NO MANIFEST ROW:"

O2 = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]
OS_ = ["/c", "/nologo", "/O2", "/Os", "/Gy", "/GS-", "/fp:fast"]
ADDR_RE = re.compile(r"sub_([0-9A-Fa-f]{8})")


def listed_sources():
    out = set()
    for p in (MANIFEST, ATTEMPTS):
        if not p.exists():
            continue
        for line in p.read_text().splitlines():
            line = line.split("#")[0].strip()
            if line:
                out.add(line.split()[0].replace("\\", "/"))
    return out


TLO = THI = 0
SECTIONS = []
# In --attempts mode the address comes from attempts.txt, which is
# authoritative, rather than from the source comment.
ATTEMPT_TARGET = {}


def sect_of(va):
    for s in SECTIONS:
        if s["va"] <= va < s["va"] + (s["vsize"] or s["rawsz"]):
            return "in %s, which build.py does not splice" % s["name"]
    return "in no section"


def target_of(path):
    """The address this source claims to decompile, or None.

    The FIRST sub_ mention only. Later ones are cross-references to callees
    and neighbours, and taking the last would silently retarget a file at a
    function it merely mentions.
    """
    m = ADDR_RE.search(path.read_text(encoding="utf-8", errors="replace"))
    return int(m.group(1), 16) if m else None


def undecorate(name):
    """`?HasLinkedLeaf@@YA_NPBULeafOwner@@@Z` -> `HasLinkedLeaf`.

    build.py and matched_table.py both select a function by testing
    `("?" + sym + "@@") in mangled`, so the bare identifier is what a
    manifest row must carry.
    """
    if name.startswith("?"):
        i = name.find("@@")
        if i > 1:
            return name[1:i]
    return name


def score(img, sizes, blob, target):
    """Best (identical, compared, size_ok, symbol) over the file's functions."""
    tsize = sizes.get(target)
    if tsize is None:
        return None
    tbytes_full = img.read(target, tsize)
    if tbytes_full is None:
        return None
    best = None
    fns = [f for f in coff_functions(blob) if trim_padding(f[1], f[2])[0]]
    nfns = len(fns)
    for name, code, mask in fns:
        code, mask = trim_padding(code, mask)
        tb, ts = tbytes_full, tsize
        grown = can_extend(img, sizes, code, mask, target, ts)
        if grown is not None:
            tb, ts = grown, len(code)
        elif can_shrink(code, mask, tb, target, ts):
            tb, ts = tb[:len(code)], len(code)
        n = min(len(code), len(tb)) // 4
        same = 0
        compared = 0
        for i in range(n):
            if not all(mask[i * 4:i * 4 + 4]):
                continue
            compared += 1
            if (struct.unpack_from(">I", tb, i * 4)[0]
                    == struct.unpack_from(">I", code, i * 4)[0]):
                same += 1
        # Rank by how CLOSE the function is to the target's size first, and
        # only then by score.
        #
        # Ranking by score alone made a file's small helper outscore the
        # candidate it was written for: `t4_span_dispatch.cpp` reported
        # "9 of 11" for a 44-byte `OutOfRange` helper while the 176-byte
        # function it exists to match was 8 of 41. That number then went
        # into attempts.txt and into the source comment, and it reads as
        # nearly-solved when it is nowhere near -- the most expensive kind
        # of wrong, because nobody re-examines a function that looks one
        # word away.
        #
        # match.py used to take the LARGEST function when given no symbol.
        # It now REFUSES instead, because that is a guess dressed as a
        # default and it scored the wrong function while reporting a better
        # number for it. This file cannot refuse -- it exists to sweep every
        # attempt without being told anything -- so it picks by closeness to
        # the target's size, which is the same intent made explicit, and
        # RETURNS THE SYMBOL it chose so the row it writes names it.
        row = (same, compared, len(code) == ts, name, nfns)
        key = (-abs(len(code) - tsize), same)
        if best is None or key > best[0]:
            best = (key, row)
    return best[1] if best else None


def main(argv):
    img = Image()
    inv = load_inventory()
    sizes = dict(inv)
    listed = listed_sources()
    WORK.mkdir(parents=True, exist_ok=True)

    # build.py splices .text and only .text. A row outside it does not just
    # go unbuilt -- build.py stops on it, and a build that stops takes the
    # negative controls with it: "wrong codegen -> hash differs" cannot
    # observe its own failure message if the build never reaches the hash.
    # One 24-byte Bink function recorded here turned three verify checks red,
    # and none of the three named the real cause. So refuse them at the door.
    global TLO, THI, SECTIONS
    SECTIONS = img.sections
    text = next(s for s in img.sections if s["name"] == ".text")
    TLO = text["va"]
    THI = text["va"] + (text["vsize"] or text["rawsz"])

    # --attempts re-checks the near-misses instead of the unrecorded files.
    # An agent that improves an existing attempt and then dies leaves a file
    # that is already listed, so the ordinary sweep skips it forever and the
    # solve is lost exactly as completely as an unwritten manifest row.
    if "--attempts" in argv:
        want = {}
        for line in ATTEMPTS.read_text().splitlines():
            line = line.split("#")[0].strip()
            if not line:
                continue
            f = line.split()
            if len(f) >= 2:
                try:
                    want[SRC / Path(f[0]).name] = int(f[1], 16)
                except ValueError:
                    pass
        todo = [p for p in sorted(want) if p.exists()]
        print("%d near-miss source(s) in attempts.txt; %d present on disk"
              % (len(want), len(todo)))
        ATTEMPT_TARGET.update(want)
    else:
        todo = []
        for p in sorted(SRC.glob("*.cpp")):
            rel = "src/" + p.name
            if rel in listed:
                continue
            todo.append(p)

        print("%d source(s) in src/; %d already recorded; %d unrecorded"
              % (len(list(SRC.glob("*.cpp"))), len(listed), len(todo)))
    if not todo:
        return 0
    print("")

    matched, near, noaddr, broken = [], [], [], []
    for p in todo:
        t = ATTEMPT_TARGET.get(p) or target_of(p)
        if t is None:
            noaddr.append(p)
            print("  %-34s no sub_XXXXXXXX comment -- cannot target" % p.name)
            continue
        if t not in sizes:
            noaddr.append(p)
            print("  %-34s %08X is not in the inventory" % (p.name, t))
            continue
        if not (TLO <= t < THI):
            noaddr.append(p)
            print("  %-34s %08X is OUTSIDE .text -- %s" % (p.name, t, sect_of(t)))
            continue

        best = None
        for flags, tag in ((O2, "/O2"), (OS_, "/O2 /Os")):
            blob, err = xdkcc.compile_obj(p, WORK / (p.stem + ".obj"),
                                          flags, WORK)
            if blob is None:
                if best is None:
                    best = ("ERR", err)
                continue
            s = score(img, sizes, blob, t)
            if s is None:
                continue
            same, compared, size_ok, name, nfns = s
            cand = (same, compared, size_ok, name, tag, nfns)
            if best is None or best[0] == "ERR" \
                    or (cand[0], cand[2]) > (best[0], best[2]):
                best = cand

        if best is None or best[0] == "ERR":
            broken.append((p, t, best[1] if best else "no functions emitted"))
            print("  %-34s %08X  WOULD NOT COMPILE" % (p.name, t))
            continue

        same, compared, size_ok, name, tag, nfns = best
        if size_ok and compared and same == compared:
            matched.append((p, t, tag, name, nfns))
            print("  %-34s %08X  MATCH  %s  (%d of %d word(s))"
                  % (p.name, t, tag, same, compared))
        else:
            near.append((p, t, same, compared, size_ok, tag))
            print("  %-34s %08X  near   %s  %d of %d word(s)%s"
                  % (p.name, t, tag, same, compared,
                     "" if size_ok else "  SIZE DIFFERS"))

    print("")
    print("%d matched, %d near-miss, %d untargetable, %d would not compile"
          % (len(matched), len(near), len(noaddr), len(broken)))
    print("  of %d unrecorded source(s)" % len(todo))

    # A near-miss that actually MATCHES is the failure this catches, and it
    # is quiet in both directions: attempts.txt understates how much is done,
    # and someone can spend a session on a function that came out days ago.
    # Seven were sitting like that -- solved by an agent, never promoted --
    # and what surfaced them was a human noticing green rows in objdiff.
    # That is not a detection mechanism.
    if "--check" in argv and "--attempts" in argv:
        if matched:
            print("")
            print("FAIL: %d row(s) in attempts.txt MATCH and should have been"
                  % len(matched))
            print("promoted to the manifest:")
            for p, t, tag, _n, _c in matched:
                print("    %-32s %08X  %s" % (p.name, t, tag))
            print("")
            print("Run:  python tools/sweep.py --attempts --write")
            return 1
        print("")
        print("no row in attempts.txt secretly matches.")
        return 0

    # WITHOUT --attempts, --check asks the OTHER question: is there finished
    # work with no row at all?
    #
    # verify.py ran only `--attempts --check`, which re-scores rows already
    # IN attempts.txt. A source that matches and is in NEITHER file was
    # invisible to the entire suite, and three were sitting there at once:
    # w4_bit1_of56.cpp (16 bytes, 4 of 4 words, no relocations, simply never
    # given a row), w4_tail_floats.cpp (deliberate -- see below), and
    # y2_hsv_to_rgb.cpp, whose near-miss row was deleted by an agent and
    # swept into a commit by `git add -A`. HANDBOOK has long said "work can
    # be finished and still have nowhere to go"; nothing checked for it.
    #
    # THE DELIBERATE CASE IS DECLARED IN THE SOURCE, not in a list here. A
    # file may opt out by containing the marker below together with its
    # reason, which keeps the exception next to the evidence and makes
    # opting out an act rather than an oversight. w4_tail_floats.cpp is the
    # real one: match.py calls it a MATCH while build.py, which RESOLVES
    # relocations instead of excusing them, sees swapped registers inside
    # four relocated words -- so neither file can hold the row.
    if "--check" in argv:
        homeless = [m for m in matched
                    if NO_ROW_MARKER not in m[0].read_text(
                        encoding="utf-8", errors="replace")]
        excused = len(matched) - len(homeless)
        if homeless:
            print("")
            print("FAIL: %d source(s) MATCH and have no row in "
                  "src/manifest.txt" % len(homeless))
            print("or src/attempts.txt, so their bytes are counted nowhere:")
            for p, t, tag, _n, _c in homeless:
                print("    %-32s %08X  %s" % (p.name, t, tag))
            print("")
            print("Add the manifest row, or -- if the row genuinely cannot")
            print("exist -- say so in the source with a line containing")
            print("    %s" % NO_ROW_MARKER)
            print("and the reason beside it.")
            return 1
        print("")
        print("no unrecorded source matches%s."
              % ("" if not excused else
                 "; %d declared %s" % (excused, NO_ROW_MARKER)))
        return 0

    if "--write" not in argv:
        print("")
        print("nothing written; pass --write to record these")
        return 0

    # In --attempts mode a solved function is in attempts.txt BY DEFINITION,
    # so checking both files made every promotion look like a duplicate of
    # its own near-miss row and silently skipped all six. The claim to guard
    # against is two different sources for one address in the MANIFEST; the
    # attempts row is the thing being replaced.
    sources = (MANIFEST,) if "--attempts" in argv else (MANIFEST, ATTEMPTS)
    already = set()
    for pth in sources:
        if pth.exists():
            for line in pth.read_text().splitlines():
                line = line.split("#")[0].strip()
                if line and len(line.split()) >= 2:
                    try:
                        already.add(int(line.split()[1], 16))
                    except ValueError:
                        pass

    rows, arows, dup = [], [], []
    for p, t, tag, name, nfns in matched:
        if t in already:
            dup.append((p, t))
            continue
        already.add(t)
        # Name the symbol whenever the object holds more than one function.
        # Without it build.py stops -- and a stopped build takes the negative
        # controls with it, so three unrelated verify checks go red and none
        # of them names the cause.
        sym = undecorate(name) if nfns > 1 else "-"
        if tag == "/O2 /Os":
            rows.append("%-32s %08X  %-22s flags=/O2,/Os,/Gy,/GS-,/fp:fast"
                        % ("src/" + p.name, t, sym))
        elif sym != "-":
            rows.append("%-32s %08X  %s" % ("src/" + p.name, t, sym))
        else:
            rows.append("%-32s %08X" % ("src/" + p.name, t))
    # In --attempts mode every near-miss examined came FROM attempts.txt, so
    # writing them back appends a second copy of each. `already` cannot catch
    # that here, because it is deliberately built from the manifest alone so
    # that promotions are not mistaken for duplicates.
    if "--attempts" not in argv:
        for p, t, _s, _c, _ok, _tag in near:
            if t in already:
                dup.append((p, t))
                continue
            already.add(t)
            arows.append("%-32s %08X" % ("src/" + p.name, t))

    if dup:
        print("")
        print("%d skipped -- another source already claims that address:"
              % len(dup))
        for p, t in dup:
            print("    %-32s %08X" % (p.name, t))

    # A promotion is a MOVE, not a copy. Without this the six solved
    # functions sat in manifest.txt and attempts.txt at once, and
    # attempts.txt then overstated what still resists -- the one number that
    # document exists to report.
    if "--attempts" in argv and rows:
        solved = set(t for _p, t, _tag, _n, _c in matched)
        keep, cut = [], 0
        for line in ATTEMPTS.read_text(encoding="utf-8").splitlines(True):
            f = line.split("#")[0].split()
            drop = False
            if len(f) >= 2:
                try:
                    drop = int(f[1], 16) in solved
                except ValueError:
                    drop = False
            if drop:
                cut += 1
            else:
                keep.append(line)
        ATTEMPTS.write_text("".join(keep), encoding="utf-8")
        print("")
        print("promoted %d row(s) out of attempts.txt" % cut)

    if rows:
        with MANIFEST.open("a", encoding="utf-8") as f:
            f.write("\n".join(rows) + "\n")
    if arows:
        with ATTEMPTS.open("a", encoding="utf-8") as f:
            f.write("\n".join(arows) + "\n")
    print("")
    print("appended %d manifest row(s) and %d attempts row(s)"
          % (len(rows), len(arows)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
