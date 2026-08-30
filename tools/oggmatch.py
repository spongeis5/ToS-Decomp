"""Compile a libogg/libvorbis release with the XDK compiler and find it in the image.

    python tools/oggmatch.py                 the trees under SDKFiles/
    python tools/oggmatch.py --min-bytes 24  demand more evidence per function
    python tools/oggmatch.py --list          what it would compile, and stop

This is the ONE piece of middleware in this image whose real source can be
had. `tools/srcfiles.py` recovers fourteen paths -- `ogg/src/framing.c` and
thirteen `vorbis/lib/*.c` -- and libogg/libvorbis are BSD-licensed and
published by Xiph.Org. Everywhere else in this project a function has to be
reconstructed from its bytes; here the bytes can be compared against the
source that produced them.

TWO THINGS ARE MEASURED, NOT ASSUMED (see FINDINGS §8a):

  * It is **FMOD's vendored copy**. The vorbis paths and the `fmod_*.cpp`
    paths are one contiguous string block, so local modification is likely
    and a file that does not match is not evidence that the release is wrong.
  * The **release is not stamped**. `Xiph`, `libVorbis` and `Vorbis I` appear
    zero times in the image; the vendor string upstream keeps in `info.c` is
    not referenced by a decoder that never packs a comment header. What dates
    the drop is FMOD's other vendored codec, `reference libFLAC 1.2.1
    20070917`, which puts libvorbis 1.2.0 and libogg 1.1.3 as the
    contemporaneous pair and 1.2.1 / 1.1.4 as the next ones.

So the release is settled BY COMPILING CANDIDATES AND COUNTING, which is what
this does. Point it at a tree, read how many functions it finds; the release
that finds more is the release. A count is only evidence next to another
count, so the output always states the denominator.

IDENTIFICATION, NOT VERIFICATION. The comparison is `libmatch`'s masked scan,
imported rather than rewritten: every word a relocation touches is ignored and
the match is judged on the rest, with a floor on how many bytes were actually
compared. That answers "these image bytes came from this code". It does NOT
answer "this source reproduces this function exactly" -- that is `match.py`,
it uses can_shrink/can_extend, and a function found here still has to go
through it before it can enter `src/manifest.txt`.
"""

import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import xdkcc
from libmatch import coff_functions, indexable, scan, MIN_UNMASKED
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent
SDK = ROOT / "SDKFiles"
WORK = ROOT / "build/ogg"

# The retail flags. The same set every other match in this project is made
# with -- there is no reason to think FMOD's vendored C was built differently
# from the rest of the image, and if it was, that is a finding this will
# surface as a wall of near-misses rather than something to guess at now.
FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast",
         # FORCED, not chosen. libvorbis's os.h has
         #     #if defined(_WIN32) && !defined(__GNUC__) && !defined(__BORLANDC__)
         # guarding an x86 `__asm { fld f / fistp i }` implementation of
         # vorbis_ftoi. The XDK compiler defines _WIN32 and neither of the
         # others, so it takes that branch and dies with C2759 "Unknown
         # opcode: fld" -- vorbisfile.c and lsp.c did not compile at all.
         # Defining __BORLANDC__ falls through to the portable
         # `(int)(f+.5)`. It is surgical: __BORLANDC__ appears exactly once
         # in either tree, on that line. Whether FMOD did the same or patched
         # os.h is NOT_MEASURED -- but only lookup.c and vorbisfile.c call
         # vorbis_ftoi, so it cannot explain a DSP file matching nothing.
         "/D__BORLANDC__"]

# The fourteen files the image names, from tools/srcfiles.py. Anything else in
# the tree is compiled too -- an unnamed file can still be present, since only
# a function carrying an assert path leaves a name behind.
NAMED = [
    "ogg/src/framing.c",
    "vorbis/lib/block.c", "vorbis/lib/codebook.c", "vorbis/lib/envelope.c",
    "vorbis/lib/floor0.c", "vorbis/lib/floor1.c", "vorbis/lib/info.c",
    "vorbis/lib/mapping0.c", "vorbis/lib/mdct.c", "vorbis/lib/psy.c",
    "vorbis/lib/res0.c", "vorbis/lib/sharedbook.c", "vorbis/lib/smallft.c",
    "vorbis/lib/vorbisfile.c",
]


def all_trees():
    """-> ([ogg dirs], [vorbis dirs]) under SDKFiles, oldest first."""
    ogg = sorted(p for p in SDK.glob("libogg-*") if p.is_dir())
    vorbis = sorted(p for p in SDK.glob("libvorbis-*") if p.is_dir())
    return ogg, vorbis


def trees(argv=()):
    """-> (ogg dir, vorbis dir). `--ogg`/`--vorbis` name one explicitly."""
    ogg, vorbis = all_trees()
    o = v = None
    if "--ogg" in argv:
        want = argv[list(argv).index("--ogg") + 1]
        o = next((p for p in ogg if want in p.name), None)
    if "--vorbis" in argv:
        want = argv[list(argv).index("--vorbis") + 1]
        v = next((p for p in vorbis if want in p.name), None)
    return (o or (ogg[-1] if ogg else None),
            v or (vorbis[-1] if vorbis else None))


def sources(ogg, vorbis):
    """[(label, path)] -- every .c worth compiling, named ones first."""
    out, seen = [], set()
    for rel in NAMED:
        base = ogg if rel.startswith("ogg/") else vorbis
        p = base / rel.split("/", 1)[1]
        if p.exists():
            out.append((rel, p))
            seen.add(p.resolve())
    for base, sub, tag in ((ogg, "src", "ogg"), (vorbis, "lib", "vorbis")):
        if base is None:
            continue
        for p in sorted((base / sub).glob("*.c")):
            if p.resolve() not in seen:
                out.append(("%s/%s/%s" % (tag, sub, p.name), p))
    return out


# FMOD is a separate library built by FMOD's own build system, so there is no
# reason its flags must be the title's. The DSP files -- psy, res0, smallft,
# floor0, mapping0, envelope -- are the float-heavy ones, and they are exactly
# the named files matching nothing at /fp:fast. That is a flags hypothesis, and
# a flags hypothesis is cheap to settle: compile the same source several ways
# and count. Which set wins is a measurement, not a preference.
SWEEP = [
    ("/O2 /fp:fast", ["/O2", "/Gy", "/GS-", "/fp:fast"]),
    ("/O2 /fp:precise", ["/O2", "/Gy", "/GS-", "/fp:precise"]),
    ("/O2 (default fp)", ["/O2", "/Gy", "/GS-"]),
    ("/O2 /Os /fp:fast", ["/O2", "/Os", "/Gy", "/GS-", "/fp:fast"]),
    ("/Ox /fp:fast", ["/Ox", "/Gy", "/GS-", "/fp:fast"]),
    ("/O1 /fp:fast", ["/O1", "/Gy", "/GS-", "/fp:fast"]),
]


def build_index(srcs, opt, inc, min_bytes):
    """Compile every source at one flag set. -> (wanted, stats, per_file)."""
    wanted, stats, per_file = {}, Counter(), {}
    flags = ["/c", "/nologo"] + opt + ["/D__BORLANDC__"] + inc
    tagsuffix = "_" + "".join(c for c in "".join(opt) if c.isalnum())
    for lbl, p in srcs:
        tag = lbl.replace("/", "_").replace(".c", "") + tagsuffix
        blob, err = xdkcc.compile_obj(p, WORK / (tag + ".obj"), flags, WORK)
        if blob is None:
            per_file[lbl] = [0, 0]
            stats["failed"] += 1
            stats.setdefault("_firsterr", None)
            if not stats.get("_msg"):
                stats["_msg"] = (lbl, (err or "").splitlines()[:1])
            continue
        fns = coff_functions(blob)
        per_file[lbl] = [len(fns), 0]
        for sym, code, mask in fns:
            stats["functions"] += 1
            key, ent = indexable(code, mask, min_bytes)
            if key is None:
                stats[ent] += 1
                continue
            stats["indexable"] += 1
            wanted.setdefault(key, []).append((lbl, sym) + ent)
    return wanted, stats, per_file


def compare(min_bytes):
    """Every (ogg, vorbis) pair under SDKFiles, at the settled flags.

    The version is not in the image (FINDINGS §8a), so it is decided the only
    way left: compile each candidate and count what the image contains. A
    release that identifies MORE of the same fourteen named files is not a
    better guess, it is the answer -- and a release that identifies fewer is
    excluded rather than merely disfavoured.
    """
    oggs, vorbises = all_trees()
    if not oggs or not vorbises:
        print("No libogg-*/ or libvorbis-*/ trees under %s." % SDK)
        return 1
    img = Image()
    inv = dict(load_inventory())
    opt = SWEEP[0][1]                      # /O2 /Gy /GS- /fp:fast -- measured
    print("flags %s, settled by --sweep\n" % " ".join(opt))
    print("%-16s %-18s %9s %7s %8s %s"
          % ("ogg", "vorbis", "indexable", "found", "at start", "named"))
    rows = []
    for o in oggs:
        for v in vorbises:
            inc = ["/I" + str(o / "include"), "/I" + str(v / "include"),
                   "/I" + str(v / "lib")]
            srcs = sources(o, v)
            wanted, stats, per_file = build_index(srcs, opt, inc, min_bytes)
            matched, _pos = scan(img, wanted)
            for _va, ent in matched.items():
                if ent[0] in per_file:
                    per_file[ent[0]][1] += 1
            named_hit = sum(1 for l, (n, m) in per_file.items()
                            if m and l in NAMED)
            at_start = sum(1 for va in matched if va in inv)
            rows.append((o.name, v.name, stats["indexable"], len(matched),
                         at_start, named_hit, per_file))
            print("%-16s %-18s %9d %7d %8d %d of %d"
                  % (o.name, v.name, stats["indexable"], len(matched),
                     at_start, named_hit, len(NAMED)))

    rows.sort(key=lambda r: (-r[3], -r[5]))
    best = rows[0]
    print("")
    print("BEST: %s + %s -- %d site(s), %d of %d named file(s)"
          % (best[0], best[1], best[3], best[5], len(NAMED)))
    if len(rows) > 1 and rows[1][3] == best[3]:
        print("TIED with %s + %s. The pair is NOT decided by this; the files"
              % (rows[1][0], rows[1][1]))
        print("that differ between them are not present in the image, or are")
        print("identical across the releases.")
    print("")
    print("Per named file, best pair:")
    pf = best[6]
    for lbl in NAMED:
        n, m = pf.get(lbl, (0, 0))
        print("  %-32s %4d compiled %4d found%s"
              % (lbl, n, m, "" if m else "   <- nothing"))
    return 0


def main(argv):
    min_bytes = MIN_UNMASKED
    if "--min-bytes" in argv:
        min_bytes = int(argv[argv.index("--min-bytes") + 1])

    if "--compare" in argv:
        return compare(min_bytes)

    ogg, vorbis = trees(argv)
    if ogg is None or vorbis is None:
        print("No libogg-*/ and libvorbis-*/ under %s." % SDK)
        print("")
        print("They are BSD-licensed and published by Xiph.Org:")
        print("  https://downloads.xiph.org/releases/ogg/")
        print("  https://downloads.xiph.org/releases/vorbis/")
        print("Unpack them there. Nothing was compiled.")
        return 1
    print("ogg    %s" % ogg.name)
    print("vorbis %s" % vorbis.name)

    srcs = sources(ogg, vorbis)
    named = sum(1 for lbl, _p in srcs if lbl in NAMED)
    print("%d source file(s), %d of them among the %d the image names"
          % (len(srcs), named, len(NAMED)))
    if "--list" in argv:
        for lbl, p in srcs:
            print("  %-34s %s" % (lbl, "NAMED" if lbl in NAMED else ""))
        return 0

    inc = ["/I" + str(ogg / "include"), "/I" + str(vorbis / "include"),
           "/I" + str(vorbis / "lib")]
    WORK.mkdir(parents=True, exist_ok=True)

    if "--sweep" in argv:
        img = Image()
        inv = dict(load_inventory())
        print("")
        print("%-20s %9s %9s %9s %s"
              % ("flags", "indexable", "found", "at start", "named files hit"))
        best = None
        for label, opt in SWEEP:
            wanted, stats, per_file = build_index(srcs, opt, inc, min_bytes)
            matched, _pos = scan(img, wanted)
            for va, ent in matched.items():
                if ent[0] in per_file:
                    per_file[ent[0]][1] += 1
            hit_named = sum(1 for l, (n, m) in per_file.items()
                            if m and l in NAMED)
            at_start = sum(1 for va in matched if va in inv)
            print("%-20s %9d %9d %9d %d of %d"
                  % (label, stats["indexable"], len(matched), at_start,
                     hit_named, len(NAMED)))
            if best is None or len(matched) > best[1]:
                best = (label, len(matched))
        print("")
        print("best: %s at %d site(s). A flag set that finds MORE of the same"
              % (best[0], best[1]))
        print("source is the one FMOD used; one that finds fewer is not a")
        print("weaker guess, it is the wrong answer. Denominator on every row.")
        return 0

    print("")
    wanted, stats = {}, Counter()
    per_file = {}
    failed = []
    for lbl, p in srcs:
        tag = lbl.replace("/", "_").replace(".c", "")
        blob, err = xdkcc.compile_obj(p, WORK / (tag + ".obj"),
                                      FLAGS + inc, WORK)
        if blob is None:
            failed.append((lbl, (err or "").splitlines()[:1]))
            per_file[lbl] = [0, 0]
            continue
        fns = coff_functions(blob)
        per_file[lbl] = [len(fns), 0]
        for sym, code, mask in fns:
            stats["functions"] += 1
            key, ent = indexable(code, mask, min_bytes)
            if key is None:
                stats[ent] += 1
                continue
            stats["indexable"] += 1
            wanted.setdefault(key, []).append((lbl, sym) + ent)

    if failed:
        print("DID NOT COMPILE -- %d of %d file(s), and they are not counted"
              % (len(failed), len(srcs)))
        for lbl, msg in failed:
            print("  %-34s %s" % (lbl, msg[0] if msg else "?"))
        print("")

    print("compiled %d function(s); %d indexable "
          "(%d too short, %d first word relocated, %d too masked at <%d B)"
          % (stats["functions"], stats["indexable"], stats["too_short"],
             stats["first_word_relocated"], stats["too_masked"], min_bytes))

    img = Image()
    inv = dict(load_inventory())
    matched, positions = scan(img, wanted)
    print("scanned %d aligned position(s)" % positions)
    print("")

    for va, ent in matched.items():
        lbl = ent[0]
        if lbl in per_file:
            per_file[lbl][1] += 1

    hit_files = [(l, n, m) for l, (n, m) in per_file.items() if m]
    hit_files.sort(key=lambda x: -x[2])
    print("%-34s %8s %8s" % ("file", "compiled", "found"))
    for lbl, n, m in hit_files:
        print("  %-32s %8d %8d%s"
              % (lbl, n, m, "   NAMED" if lbl in NAMED else ""))
    quiet = [(l, n) for l, (n, m) in per_file.items() if not m and n]
    print("")
    print("%d file(s) contributed a match; %d compiled functions but matched "
          "nothing" % (len(hit_files), len(quiet)))
    for lbl, n in sorted(quiet)[:12]:
        print("  %-32s %8d      0%s"
              % (lbl, n, "   NAMED" if lbl in NAMED else ""))
    if len(quiet) > 12:
        print("  ... and %d more" % (len(quiet) - 12))

    at_start = sum(1 for va in matched if va in inv)
    print("")
    print("SITES IDENTIFIED IN THE IMAGE : %6d" % len(matched))
    print("  at a known function start   : %6d" % at_start)
    print("  not a known function start  : %6d" % (len(matched) - at_start))
    print("  of %d indexable function(s) compiled from this release"
          % stats["indexable"])
    print("")
    print("A COUNT IS ONLY EVIDENCE NEXT TO ANOTHER COUNT. Run this again")
    print("against libvorbis 1.2.1 / libogg 1.1.4 and compare; the release")
    print("that finds more is the one FMOD vendored. Nothing here is a match")
    print("in the sense src/manifest.txt means -- these are IDENTIFICATIONS,")
    print("and each still has to pass tools/match.py to be claimed.")

    out = ROOT / "build/ogg_sites.txt"
    with out.open("w") as f:
        f.write("# address size file symbol -- masked identification, "
                "tools/oggmatch.py\n")
        f.write("# tree: %s %s\n" % (ogg.name, vorbis.name))
        for va in sorted(matched):
            lbl, sym, size, un = matched[va]
            f.write("%08X %6d %-32s %s\n" % (va, size, lbl, sym))
    print("")
    print("wrote %s" % out.relative_to(ROOT).as_posix())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
