"""Find function starts and call edges from the image alone, without Ghidra.

    python tools/discover.py            report, and write build/discovered.txt
    python tools/discover.py --compare  score against Ghidra's answer

Ghidra contributes exactly two things to this project: 4,499 function starts
that `.pdata` does not list, and the call graph. Neither needs a decompiler.
A `bl` names its target unambiguously in the instruction word, so a linear
sweep of the executable sections finds every called function, and the same
sweep yields the edges.

Why bother replacing it:

  * Ghidra cannot decode VMX128 (issue #2094, open since 2020) and this image
    contains 44,956 VMX128 instructions, so its sweep is blind in places ours
    is not.
  * Its function bodies are computed from REACHABLE code, which is what makes
    171 recorded sizes short by the unreachable `blr` after a tail call.
  * It needs tuning scripts here to stop its heuristics making things worse.

WHAT THIS DOES NOT DO: it will not find a function that is never called and
never referenced -- one reached only through a computed jump, or only from a
vtable this does not recognise. That is a real limitation and it is why the
answer is compared against Ghidra's rather than assumed to replace it.

METHOD, and it is deliberately dumb:

  * every 4-byte word in an executable section is examined
  * opcode 18 with LK=1 (`bl`) gives a call edge and a function start
  * opcode 18 with LK=0 (`b`) gives a tail-call candidate: counted as a start
    only when the target lies outside the .pdata function containing it
  * a word that decodes as a branch but points outside the executable
    sections is discarded -- data in .text decodes as something

No reachability, no recursion, no heuristics about prologues.
"""

import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_functions, load_inventory

OUT = Path("build/discovered.txt")
GHIDRA = Path("build/ghidra_fn_v2.txt")


def sign_extend(v, bits):
    m = 1 << (bits - 1)
    return (v ^ m) - m


def exec_ranges(img):
    out = []
    for s in img.sections:
        if s["exec"] and (s["vsize"] or s["rawsz"]):
            out.append((s["va"], s["va"] + (s["vsize"] or s["rawsz"])))
    return out


def sweep(img):
    """-> (calls, tails, edges, examined) over every executable word."""
    ranges = exec_ranges(img)
    calls, tails = set(), set()
    edges = defaultdict(set)
    examined = 0
    undecodable = 0

    pdata = load_functions()
    starts = [a for a, _s in pdata]
    sizes = dict(pdata)

    def containing(va):
        """The .pdata function containing va, by binary search."""
        lo, hi = 0, len(starts) - 1
        best = None
        while lo <= hi:
            mid = (lo + hi) // 2
            if starts[mid] <= va:
                best = starts[mid]
                lo = mid + 1
            else:
                hi = mid - 1
        if best is not None and va < best + sizes.get(best, 0):
            return best
        return None

    for lo, hi in ranges:
        data = img.read(lo, hi - lo)
        if data is None:
            continue
        n = len(data) // 4
        words = struct.unpack(">%dI" % n, data[:n * 4])
        for i, w in enumerate(words):
            va = lo + i * 4
            if (w >> 26) & 0x3F != 18:
                continue
            examined += 1
            aa = w & 2
            lk = w & 1
            li = sign_extend(w & 0x03FFFFFC, 26)
            target = (li & 0xFFFFFFFF) if aa else ((va + li) & 0xFFFFFFFF)
            if not any(a <= target < b for a, b in ranges):
                undecodable += 1
                continue
            if lk:
                calls.add(target)
                src = containing(va)
                if src is not None:
                    edges[src].add(target)
            else:
                home = containing(va)
                if home is None or not (home <= target
                                        < home + sizes.get(home, 0)):
                    tails.add(target)
                    if home is not None:
                        edges[home].add(target)
    return calls, tails, edges, examined, undecodable


def data_pointers(img, ranges, known, interior=None):
    """Words in NON-executable sections that point at code.

    A virtual function is never the target of a `bl`, so the branch sweep
    cannot see one. Its address is in a vtable instead. This finds those
    without needing to identify vtables as such: a word in a data section
    whose value is a known function start is a reference to it, and one that
    lands somewhere not yet known is a candidate for a new start.

    An integer or a float can look like a code address, so candidates are
    filtered by two things that are impossible for a real function start
    rather than merely unlikely:

      * a PowerPC instruction is 4-byte ALIGNED by construction, so a value
        with low bits set cannot be an entry point;
      * a start strictly INSIDE a .pdata function contradicts the compiler's
        own unwind table, which is authoritative about extents.

    Without those two filters this admitted 148 impossible candidates out of
    3,620 (4.1%), and they showed up downstream as inventory entries whose
    extent overran the next "function" -- including starts at addresses like
    82106901, which is not even aligned.

    `interior(va)` should return the containing .pdata function or None.
    """
    hits, confirmed = set(), 0
    total = rejected_align = rejected_interior = 0
    for s in img.sections:
        if s["exec"] or not (s["rawsz"] or s["vsize"]):
            continue
        size = s["vsize"] or s["rawsz"]
        data = img.read(s["va"], size)
        if data is None:
            continue
        n = len(data) // 4
        for w in struct.unpack(">%dI" % n, data[:n * 4]):
            if not any(a <= w < b for a, b in ranges):
                continue
            total += 1
            if w in known:
                confirmed += 1
                continue
            if w & 3:
                rejected_align += 1
                continue
            if interior is not None and interior(w):
                rejected_interior += 1
                continue
            hits.add(w)
    return hits, confirmed, total, rejected_align, rejected_interior


def main(argv):
    img = Image()
    pdata = load_functions()
    pset = set(a for a, _s in pdata)

    print("Sweeping every word in the executable sections for branches.\n")
    calls, tails, edges, examined, undecodable = sweep(img)

    print("  opcode-18 branch words examined      %8d" % examined)
    print("  discarded, target outside .text      %8d" % undecodable)
    print("  distinct `bl` targets                %8d" % len(calls))
    print("  distinct outward `b` targets         %8d" % len(tails))
    found = calls | tails
    print("  union                                %8d" % len(found))
    print("")

    # VALIDATION FIRST. A sweep that cannot rediscover the table we already
    # trust has no business reporting anything about the functions it adds.
    hit = len(found & pset)
    print("VALIDATION against .pdata, which this must largely rediscover:")
    print("  .pdata function starts               %8d" % len(pset))
    print("  of those, found by the sweep         %8d  (%.1f%%)"
          % (hit, 100.0 * hit / len(pset)))
    print("  .pdata starts the sweep MISSED       %8d" % (len(pset) - hit))
    print("  (a miss is a function nothing calls with `bl` -- expected for")
    print("   entry points, virtual-only methods and unreferenced code)")
    print("")

    print("  starts found that .pdata does NOT list  %5d" % len(found - pset))
    print("")

    ranges = exec_ranges(img)
    known = pset | found

    import bisect
    pstarts = sorted(pset)
    psize = dict(pdata)

    def interior(va):
        """The .pdata function strictly containing va, or None."""
        i = bisect.bisect_right(pstarts, va) - 1
        if i < 0:
            return None
        s = pstarts[i]
        return s if s < va < s + psize.get(s, 0) else None

    (ptr_new, ptr_confirmed, ptr_total,
     rej_align, rej_interior) = data_pointers(img, ranges, known, interior)
    print("DATA POINTERS -- for functions only ever reached through a vtable,")
    print("which no `bl` names:")
    print("  aligned data words pointing into code %8d" % ptr_total)
    print("  of those, on an ALREADY-KNOWN start   %8d  (%.1f%%)"
          % (ptr_confirmed, 100.0 * ptr_confirmed / max(ptr_total, 1)))
    print("  rejected, not 4-byte aligned          %8d" % rej_align)
    print("  rejected, inside a .pdata function    %8d" % rej_interior)
    print("  landing somewhere not yet known       %8d" % len(ptr_new))
    print("  (the hit rate above is the check: if most data words that look")
    print("   like code addresses ARE known starts, the rest are credible.")
    print("   The two rejections are impossibilities, not improbabilities:")
    print("   a PowerPC entry point is aligned, and .pdata is authoritative")
    print("   about the extent of the functions it lists.)")
    print("")

    found |= ptr_new
    beyond = sorted(found - pset)
    print("  TOTAL starts beyond .pdata            %8d" % len(beyond))
    OUT.write_text("# address  (function starts: branch sweep + data pointers)\n"
                   + "".join("%08X\n" % a for a in sorted(found)))
    print("  -> %s" % OUT)
    print("")

    # SIZES. .pdata carries real sizes; a discovered function's extent is the
    # distance to the next start, minus trailing padding. That is deliberately
    # NOT how Ghidra does it -- Ghidra computes a body from reachable code,
    # which is what leaves 171 sizes short by the unreachable `blr` after a
    # tail call (see MATCHED.md).
    all_starts = sorted(pset | found)
    sizes_out, hi_ranges = {}, exec_ranges(img)
    pd_size = dict(pdata)
    for i, a in enumerate(all_starts):
        end = all_starts[i + 1] if i + 1 < len(all_starts) else None
        if end is None:
            end = next((h for lo, h in hi_ranges if lo <= a < h), a + 4)
        n = end - a
        if n <= 0 or n > 0x20000:
            continue
        blob = img.read(a, n)
        if blob is None:
            continue
        while n >= 4 and blob[n - 4:n] in (bytes(4), bytes([0x60, 0, 0, 0])):
            n -= 4
        if n >= 4:
            sizes_out[a] = n

    agree = tot_cmp = 0
    for a, s in pd_size.items():
        if a in sizes_out:
            tot_cmp += 1
            agree += (sizes_out[a] == s)
    print("SIZES, validated against the %d .pdata rows that carry one:" % tot_cmp)
    print("  extent-to-next-start, padding trimmed, agrees  %6d  (%.1f%%)"
          % (agree, 100.0 * agree / max(tot_cmp, 1)))
    print("  disagrees                                      %6d" % (tot_cmp - agree))
    print("  (a disagreement is usually alignment padding between functions")
    print("   that is not a zero or a nop, so this is a floor, not a defect)")
    print("")

    ordered = [a for a in all_starts if a in sizes_out or a in pd_size]
    final = [(a, pd_size.get(a, sizes_out.get(a, 0))) for a in ordered]
    overlap = [(final[i][0], final[i][1], final[i + 1][0])
               for i in range(len(final) - 1)
               if final[i][0] + final[i][1] > final[i + 1][0]]
    # An overlap is only a defect if the interior address is NOT a real
    # secondary entry point. The register save/restore helpers legitimately
    # have many: __restgprlr is one 84-byte body with 15 `bl`-able entries
    # (FINDINGS 7e). A `bl` naming an interior address is evidence it IS an
    # entry point; a data word landing there is not, which is why those are
    # rejected before this point.
    ov_branch = [x for x in overlap if x[2] in found]
    ov_other = [x for x in overlap if x[2] not in found]
    print("OVERLAP CHECK on the emitted inventory:")
    print("  entries                               %8d" % len(final))
    print("  whose extent overruns the next start  %8d" % len(overlap))
    print("    interior address is a BRANCH target %8d  <- secondary entry"
          % len(ov_branch))
    print("    interior address is not             %8d  <- unexplained"
          % len(ov_other))
    for a, s, n in ov_other[:5]:
        print("      %08X + %d overruns %08X" % (a, s, n))
    print("")

    inv_out = Path("build/discovered_inventory.txt")
    inv_out.write_text("# address size  (.pdata sizes where known, else "
                       "extent-to-next-start with padding trimmed)\n"
                       + "".join("%08X %d\n" % (a, pd_size.get(a, sizes_out[a]))
                                 for a in all_starts if a in sizes_out
                                 or a in pd_size))
    cg_out = Path("build/discovered_callgraph.txt")
    cg_out.write_text("# caller callee\n" + "".join(
        "%08X %08X\n" % (s, d) for s in sorted(edges) for d in sorted(edges[s])))
    print("  inventory  -> %s  (%d function(s))" % (inv_out, len(all_starts)))
    print("  call graph -> %s  (%d edge(s) from %d caller(s))"
          % (cg_out, sum(len(v) for v in edges.values()), len(edges)))
    print("")

    if "--compare" in argv[1:]:
        # Read GHIDRA'S OWN export, not load_inventory(). The inventory is now
        # built from this very sweep, so comparing against it compares
        # discovery with itself and reports a perfect score for nothing --
        # which is exactly what it did once the default was switched.
        if not GHIDRA.exists():
            print("COMPARISON skipped: %s is not present." % GHIDRA)
            print("  Nothing in the pipeline needs it; it is only the")
            print("  cross-check. Re-export it from Ghidra to compare.")
            return 0
        gh = set()
        for line in GHIDRA.read_text().splitlines():
            if line.startswith("#") or not line.strip():
                continue
            try:
                gh.add(int(line.split()[0], 16))
            except (ValueError, IndexError):
                pass
        gset = gh - pset
        bset = set(beyond)
        print("COMPARISON with Ghidra's contribution (from %s):" % GHIDRA.name)
        print("  Ghidra function starts               %8d" % len(gh))
        print("  functions Ghidra adds beyond .pdata  %8d" % len(gset))
        print("  functions this sweep adds            %8d" % len(bset))
        print("  found by BOTH                        %8d" % len(gset & bset))
        print("  only Ghidra                          %8d" % len(gset - bset))
        print("  only this sweep                      %8d" % len(bset - gset))
        if gset:
            print("")
            print("  this sweep recovers %.1f%% of what Ghidra adds"
                  % (100.0 * len(gset & bset) / len(gset)))
        extra = sorted(bset - gset)
        if extra:
            print("")
            print("  starts this sweep found and Ghidra did not, first 10:")
            for a in extra[:10]:
                print("    %08X  called, but absent from Ghidra's list" % a)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
