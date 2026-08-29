"""Group functions into probable translation units.

    python tools/segment.py --validate     score the rule against known objects
    python tools/segment.py                segment the image, write build/segments.txt
    python tools/segment.py 82677028 ...   which segment an address is in, and
                                           what else is in it

WHY: fifteen matched sources each invent their own `struct` with `char
pad00[132]` filler, and two of them may describe the same class without
knowing. Before a shared type system can exist, something has to say which
functions belong together.

THE EVIDENCE: the linker lays out one object file's functions together, so
adjacency is the signal. That is a claim with a ground truth to check it
against -- `libmatch.py` matched 6,541 functions whose owning OBJECT FILE is
known by byte comparison, so a boundary rule can be SCORED rather than
asserted. `--validate` does exactly that and prints the table.

THE RESULT, stated plainly because it is mostly negative: adjacency alone is
a POOR boundary signal on this image. Scored pairwise against the known
objects --

    rule                          precision   recall
    gap <= 4                        55.1%       47.0%
    gap <= 64                       23.6%       62.2%
    gap <= 4  AND call-related      89.1%        6.3%

-- there is no threshold that is both accurate and complete. Requiring the
call graph to agree lifts precision from 55% to 89%, which is what the default
uses, but recall falls to 6%.

(An earlier version of this table read 72.2% for the first row. It took the
function sizes from the inventory, and 88 of the 6,541 matched functions are
not inventory entries at all -- libmatch scans every aligned position, so it
matches where no start is recorded. Those fell back to a fabricated 4 bytes,
which inflates the gap to the next function and manufactures boundaries that
are not there. Sizes now come from lib_matches.txt, which records the real
one. The default arm was unaffected; adjacency alone was overstated.)

So **this is a hint generator, not a partition.** A segment of several
functions that call each other and sit together is good evidence they share a
translation unit. The absence of a segment is no evidence at all.

The low recall is partly an artifact of the metric: 225,009 true same-object
pairs come from 610 objects, and one object with 208 functions contributes
21,528 of them, so breaking a few large objects costs most of the recall.

WHAT NO CONTIGUITY RULE CAN DO: 90 known objects have functions more than 4 KB
apart and 8 pairs overlap in address order. With /Gy every function is its own
COMDAT and the linker is free to interleave and to fold identical ones.

BEFORE THE MEASUREMENT WAS RIGHT: a first version scored "segments containing
only one object", which a singleton satisfies for free -- so the strictest
threshold won by shattering the image into 8,770 one-function segments, and
reported 92.7% precision for a segmentation that says nothing. The metric was
rewarding the failure it existed to detect. Pairwise metrics cannot be gamed
that way, and replacing it turned an apparent success into the honest negative
above.
"""

import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory, in_xdk

LIBMATCH = Path("build/lib_matches.txt")
SRCFILES = Path("build/source_files.txt")
OUT = Path("build/segments.txt")
DEFAULT_T = 4
DEFAULT_CG = True


def known_objects():
    """address -> ("lib!object", size), for functions matched byte-for-byte.

    The SIZE comes from lib_matches.txt, not from the inventory. 88 of the
    6,541 matched functions are not inventory entries at all -- libmatch scans
    every aligned position, so it can match where no start is recorded -- and
    falling back to a fabricated 4 bytes inflates the gap to the next function
    and forces boundaries that are not there.
    """
    out = {}
    if not LIBMATCH.exists():
        return out
    for line in LIBMATCH.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        if len(f) < 5:
            continue
        out.setdefault(int(f[0], 16), (f[2] + "!" + f[3], int(f[1])))
    return out


def source_paths():
    """address -> source path, from strings the code forms addresses to."""
    out = {}
    if not SRCFILES.exists():
        return out
    for line in SRCFILES.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split("\t")
        if len(f) < 3:
            continue
        for a in f[2].split():
            try:
                out[int(a, 16)] = f[0]
            except ValueError:
                pass
    return out


def call_graph():
    """(callees, callers) as address -> set, from the discovery sweep."""
    out, rev = defaultdict(set), defaultdict(set)
    p = Path("build/discovered_callgraph.txt")
    if not p.exists():
        p = Path("build/callgraph.txt")
    if not p.exists():
        return out, rev
    for line in p.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        if len(f) < 2:
            continue
        try:
            a, b = int(f[0], 16), int(f[1], 16)
        except ValueError:
            continue
        out[a].add(b)
        rev[b].add(a)
    return out, rev


def related(a, b, edges, rev):
    """Does the call graph tie these two together?

    MEASURED on adjacent pairs whose true object is known:

        call each other      92.4% are the same object
        share a callee       91.9%
        share a caller       86.2%
        no call relation     75.7%
        (baseline, all adjacent pairs: 85.4%)

    So the first two are worth requiring and the last is worth excluding.
    Sharing a caller is barely above baseline and is not used.
    """
    if b in edges[a] or a in edges[b]:
        return True
    return bool(edges[a] & edges[b])


def cluster(funcs, threshold, edges=None, rev=None):
    """funcs: sorted [(addr, size)] -> [[(addr, size), ...], ...]

    Two consecutive functions join the same segment when they are within
    `threshold` bytes AND, if a call graph was supplied, the call graph ties
    them together. The conjunction is the point: adjacency alone puts
    unrelated objects together roughly a quarter of the time.
    """
    segs, cur = [], []
    for a, s in funcs:
        if cur:
            prev_a, prev_s = cur[-1]
            gap = a - (prev_a + prev_s)
            join = (0 <= gap <= threshold)
            if join and edges is not None:
                join = related(prev_a, a, edges, rev)
            if not join:
                segs.append(cur)
                cur = []
        cur.append((a, s))
    if cur:
        segs.append(cur)
    return segs


def validate():
    owner = known_objects()
    if not owner:
        print("%s missing -- run tools/libmatch.py --all first" % LIBMATCH)
        return 1
    funcs = sorted((a, owner[a][1]) for a in owner)
    labels = [owner[a][0] for a, _s in funcs]
    nobj = len(set(labels))

    print("Scoring boundary rules against %d functions whose owning object"
          % len(funcs))
    print("is known by byte match, across %d objects.\n" % nobj)

    # PAIRWISE metrics. An earlier version of this counted "segments that
    # contain only one object", which a singleton satisfies for free -- so
    # the strictest threshold scored best by shattering the image into 8,770
    # one-function segments. That is a metric rewarding the failure it is
    # supposed to detect. Pairs cannot be gamed that way: a segment of one
    # contributes no pairs at all.
    #
    #   precision = of the function PAIRS put in one segment, how many really
    #               share an object   (a wrong MERGE costs precision)
    #   recall    = of the pairs that really share an object, how many were
    #               put together      (a wrong SPLIT costs recall)
    def pairs_of(groups):
        n = 0
        for g in groups:
            n += len(g) * (len(g) - 1) // 2
        return n

    by_obj = defaultdict(list)
    for a, _s in funcs:
        by_obj[owner[a][0]].append(a)
    truth_pairs = pairs_of(by_obj.values())

    edges, rev = call_graph()
    print("  %-9s %-9s %-11s %-9s %-9s %-10s"
          % ("gap <=", "callgraph", "precision", "recall", "F1", "segments"))
    best = None
    arms = ([(T, False) for T in (0, 4, 16, 64, 256, 1024)]
            + [(T, True) for T in (4, 16, 64, 256, 1024, 1 << 20)])
    for T, use_cg in arms:
        segs = (cluster(funcs, T, edges, rev) if use_cg
                else cluster(funcs, T))
        pred_pairs = pairs_of(segs)
        agree = 0
        for seg in segs:
            c = defaultdict(int)
            for a, _s in seg:
                c[owner[a][0]] += 1
            for _o, k in c.items():
                agree += k * (k - 1) // 2
        p = agree / pred_pairs if pred_pairs else 1.0
        r = agree / truth_pairs if truth_pairs else 1.0
        f1 = 2 * p * r / (p + r) if (p + r) else 0.0
        if best is None or f1 > best[2]:
            best = (T, use_cg, f1)
        print("  %-9s %-9s %-11s %-9s %-9s %-10d"
              % ("%d" % T if T < (1 << 20) else "any",
                 "required" if use_cg else "-",
                 "%.1f%%" % (100 * p), "%.1f%%" % (100 * r),
                 "%.3f" % f1, len(segs)))
    print("")
    print("  %d true same-object pairs across %d objects." % (truth_pairs, nobj))
    print("  Best F1: gap <= %s, call graph %s."
          % (best[0] if best[0] < (1 << 20) else "any",
             "required" if best[1] else "not used"))
    print("")
    print("A wrong MERGE invents a false type identity that is hard to notice;")
    print("a wrong SPLIT only costs duplicated effort. So precision is worth")
    print("more than recall here, and the default sits above the F1 optimum.")
    return 0


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    if "--validate" in argv[1:]:
        return validate()
    T = DEFAULT_T
    if "--threshold" in argv[1:]:
        T = int(argv[argv.index("--threshold") + 1])
    use_cg = DEFAULT_CG and "--no-callgraph" not in argv[1:]
    edges, rev = call_graph() if use_cg else (None, None)

    inv = sorted(load_inventory())
    owner = known_objects()
    paths = source_paths()
    segs = cluster(inv, T, edges, rev)

    def label(seg):
        objs = set(owner[a][0] for a, _s in seg if a in owner)
        if len(objs) == 1:
            return "lib:" + objs.pop()
        if len(objs) > 1:
            return "lib:MIXED(%d)" % len(objs)
        ps = set(paths[a] for a, _s in seg if a in paths)
        if len(ps) == 1:
            return "src:" + ps.pop()
        if len(ps) > 1:
            return "src:MIXED(%d)" % len(ps)
        return ""

    if args:
        want = [int(a, 16) for a in args]
        for va in want:
            hit = next((s for s in segs
                        if s[0][0] <= va <= s[-1][0] + s[-1][1]), None)
            if hit is None or not any(a == va for a, _s in hit):
                print("%08X is not a known function start" % va)
                continue
            lab = label(hit)
            print("%08X  segment of %d function(s), %08X..%08X%s"
                  % (va, len(hit), hit[0][0], hit[-1][0] + hit[-1][1],
                     "   " + lab if lab else ""))
            for a, s in hit:
                mark = " <--" if a == va else ""
                extra = (owner[a][0] if a in owner else paths.get(a, ""))
                print("    %08X  %5d B  %s%s" % (a, s, extra, mark))
            print("")
        return 0

    named = sum(1 for s in segs if label(s))
    game = [s for s in segs if not in_xdk(s[0][0])]
    sizes = sorted(len(s) for s in segs)
    with OUT.open("w") as f:
        f.write("# segment_index start end nfuncs label   (gap threshold %d)\n" % T)
        for i, s in enumerate(segs):
            f.write("%d %08X %08X %d %s\n"
                    % (i, s[0][0], s[-1][0] + s[-1][1], len(s), label(s) or "-"))

    print("Segmented %d functions with gap threshold %d.\n" % (len(inv), T))
    print("  segments                       %6d" % len(segs))
    print("  with a label from a lib match  %6d" % named)
    print("  outside the known XDK bands    %6d   <- the game's own" % len(game))
    print("  median functions per segment   %6d" % sizes[len(sizes) // 2])
    print("  largest segment                %6d functions" % sizes[-1])
    print("  singletons                     %6d" % sum(1 for x in sizes if x == 1))
    print("")
    print("  -> %s" % OUT)
    print("")
    print("  Rule: gap <= %d%s. Measured 89.2%% precision at 6.2%% recall"
          % (T, " AND call-graph related" if use_cg else ""))
    print("  against the known objects -- see --validate. A segment is good")
    print("  evidence that its functions share a translation unit; the absence")
    print("  of one is no evidence at all.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
