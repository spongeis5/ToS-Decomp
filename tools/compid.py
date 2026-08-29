"""Read the `@comp.id` stamp out of COFF objects.

Every object MSVC emits carries an absolute symbol `@comp.id` whose value is

    (product id << 16) | build

-- the same pair the linker later tallies into the image's Rich header. So the
Rich header is a SUM over objects, and this is the per-object term. Reading
both is a genuine cross-check: they come from different places (the linker's
census in the DOS stub vs. each compiler's own stamp in each object) and can
disagree.

Three uses:

    python tools/compid.py --libs           census every XDK library
    python tools/compid.py <file.lib|.obj>  one archive or object
    python tools/compid.py --join           prodids of the objects that
                                            libmatch.py matched into the image

`--join` is the one that matters for methodology. The retail Rich header says
54 objects were compiled /GL. If any object libmatch matched byte-for-byte
into the retail image turned out to carry a /GL product id, that reading would
be wrong -- a /GL object holds no machine code to match with.
"""

import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from libmatch import archive_members, LIBDIR

COMP_ID = b"@comp.id"

# Measured with tools/rich_calibrate.py against this XDK, not looked up.
KNOWN = {
    131: "C, no /GL",
    132: "C++, no /GL",
    137: "C, /GL (link-time codegen)",
    138: "C++, /GL (link-time codegen)",
    145: "linker",
    146: "export descriptor",
}


def comp_id(blob):
    """-> (prodid, build) or None if the object carries no @comp.id."""
    if len(blob) < 20:
        return None
    try:
        _mach, _nsec, _ts, psym, nsym, _osz, _ch = struct.unpack_from(
            "<HHIIIHH", blob, 0)
    except struct.error:
        return None
    if not psym or not nsym or psym + nsym * 18 > len(blob):
        return None
    off = psym
    i = 0
    while i < nsym:
        name = blob[off:off + 8]
        value = struct.unpack_from("<I", blob, off + 8)[0]
        naux = blob[off + 17]
        if name == COMP_ID:
            return ((value >> 16) & 0xFFFF, value & 0xFFFF)
        i += 1 + naux
        off += 18 * (1 + naux)
    return None


def objects_of(path):
    """Yield (member-name, blob) for a .lib, or the single object for a .obj."""
    data = path.read_bytes()
    if data[:8] == bytes([0x21, 0x3C, 0x61, 0x72, 0x63, 0x68, 0x3E, 0x0A]):
        for name, blob in archive_members(data):
            yield name, blob
    else:
        yield path.name, data


def census(paths):
    total = Counter()
    unstamped = 0
    nobj = 0
    per_lib = defaultdict(Counter)
    for p in paths:
        try:
            for name, blob in objects_of(p):
                nobj += 1
                cid = comp_id(blob)
                if cid is None:
                    unstamped += 1
                    per_lib[p.name][("none", 0)] += 1
                    continue
                total[cid] += 1
                per_lib[p.name][cid] += 1
        except (ValueError, struct.error, OSError) as e:
            print("  %s: unreadable (%s)" % (p.name, e))
    return total, per_lib, nobj, unstamped


def report(total, nobj, unstamped):
    print("%d object(s) examined, %d carry no @comp.id" % (nobj, unstamped))
    print("")
    print("  %-7s %-7s %-8s %s" % ("prodid", "build", "objects", "measured meaning"))
    for (prodid, build), n in sorted(total.items(),
                                     key=lambda kv: (-kv[1], kv[0])):
        print("  %-7d %-7d %-8d %s"
              % (prodid, build, n, KNOWN.get(prodid, "NOT_MEASURED")))


def main(argv):
    args = argv[1:]
    if not args:
        print(__doc__)
        return 1

    if args[0] == "--join":
        src = Path("build/lib_matches.txt")
        if not src.exists():
            print("%s missing -- run tools/libmatch.py --all first" % src)
            return 1
        want = defaultdict(set)
        rows = 0
        for line in src.read_text().splitlines():
            if line.startswith("#") or not line.strip():
                continue
            f = line.split()
            if len(f) < 5:
                continue
            rows += 1
            want[f[2]].add(f[3])
        cache = {}
        found = Counter()
        missing = 0
        for lib, members in sorted(want.items()):
            p = LIBDIR / lib
            if not p.exists():
                missing += len(members)
                continue
            if lib not in cache:
                cache[lib] = {n: comp_id(b) for n, b in objects_of(p)}
            for m in members:
                cid = cache[lib].get(m)
                if cid is None:
                    missing += 1
                else:
                    found[cid] += 1
        nobj = sum(len(v) for v in want.values())
        print("%d matched function(s) from %d distinct object(s) in %d librar(ies)"
              % (rows, nobj, len(want)))
        print("%d object(s) could not be stamped" % missing)
        print("")
        print("  %-7s %-7s %-8s %s" % ("prodid", "build", "objects", "measured meaning"))
        for (prodid, build), n in sorted(found.items(), key=lambda kv: -kv[1]):
            print("  %-7d %-7d %-8d %s"
                  % (prodid, build, n, KNOWN.get(prodid, "NOT_MEASURED")))
        ltcg = sum(n for (p, _b), n in found.items() if p in (137, 138))
        print("")
        if ltcg:
            print("%d matched object(s) carry a /GL product id. That CONTRADICTS"
                  % ltcg)
            print("the reading that /GL objects hold no matchable machine code.")
            return 2
        print("No matched object carries a /GL product id (137/138), which is")
        print("what a byte-for-byte match into the image requires: a /GL object")
        print("holds intermediate language, not PowerPC code.")
        return 0

    if args[0] == "--libs":
        paths = sorted(LIBDIR.glob("*.lib"))
        if not paths:
            print("no libraries under %s" % LIBDIR)
            return 1
        total, per_lib, nobj, unstamped = census(paths)
        report(total, nobj, unstamped)
        interesting = {137, 138, 147, 149}
        print("")
        print("libraries carrying prodids %s:" % sorted(interesting))
        any_found = False
        for lib in sorted(per_lib):
            hits = {k: v for k, v in per_lib[lib].items()
                    if k[0] in interesting}
            if hits:
                any_found = True
                print("  %-28s %s" % (lib, " ".join(
                    "%d/%d x%d" % (p, b, n) for (p, b), n in sorted(hits.items()))))
        if not any_found:
            print("  none")
        return 0

    paths = [Path(a) for a in args]
    total, _per, nobj, unstamped = census(paths)
    report(total, nobj, unstamped)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
