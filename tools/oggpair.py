"""Find Ogg/Vorbis functions the masked scan cannot, by pairing on LENGTH.

    python tools/oggpair.py            test every pair, print the verdicts
    python tools/oggpair.py --write    write build/ogg_pairs.txt

`tools/oggmatch.py` identifies by a masked key scan: the first word must be
unrelocated, sixteen bytes must survive the mask, and every unmasked run must
agree exactly. That is the right test for "where in eight megabytes did this
code land", and it is strict -- one differing word anywhere kills it.

This asks a differently-shaped question, and only inside the two address
bands the identified sites already bracket: which compiled upstream function
has the SAME TRIMMED LENGTH as this unmatched image function? Length is a
coarse fingerprint and it produces mostly noise, but it is a fingerprint the
masked scan does not use at all, so the two fail in different places.

WHAT IT FOUND, and the denominator, because a hit rate this low is the
result: 340 pairs tested over 59 unmatched functions in the two bands, 2
matched, 338 differed, 0 unmeasured. `825BFEF0` is `oggpack_look` (200
bytes) and `825BE5B0` is `ogg_stream_packetout` (8 bytes). Both are real and
both were invisible to the scan.

So the flags are not what is holding the rest back either -- `oggmatch.py
--sweep` puts /O2 /fp:fast at 37 sites and every other flag set at 37 or
fewer -- and neither is the release, which the same counting settles at
libogg 1.1.3 + libvorbis 1.2.0 against 1.1.4 + 1.2.2's 28. What is left in
those bands is FMOD's own code and FMOD's edits to vorbis, and it has to be
read rather than obtained.

A pair is a WORK LIST ENTRY, not a claim. Every one is put through
`tools/match.py`, whose verdict is read with `verify.classify_match` rather
than by looking for a word in the output -- five tools have now disagreed
with verify.py by writing their own version of that, and the outcome a
home-grown test loses is "it never compared anything at all".

Reads the .obj files `tools/oggmatch.py` already wrote under `build/ogg/`.
Run that first; this compiles nothing of its own.
"""

import subprocess
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from libmatch import coff_functions, trim_padding
from peimage import load_inventory
from verify import classify_match

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "build/ogg"
OUT = ROOT / "build/ogg_pairs.txt"
MANIFEST = ROOT / "src/manifest.txt"
VENDOR = "thirdparty/ogg_vorbis"

# The two sub-bands the identified sites bracket, with 74 KB of FMOD's own
# code between them. Searching outside them would pair every 12-byte
# accessor in the image against `ogg_page_version`.
BANDS = ((0x825A8868, 0x825AB5A8), (0x825BD920, 0x825C491C))

# Identical to what oggmatch.py compiled with and what ogg_rows.py writes:
# a row built differently from the way the claim was tested is a row that
# was never tested.
FLAGS = ("/c /nologo /O2 /Gy /GS- /fp:fast /D__BORLANDC__"
         " /I" + VENDOR + "/ogg/include"
         " /I" + VENDOR + "/vorbis/include"
         " /I" + VENDOR + "/vorbis/lib")

# The sweep writes one object per flag set beside the plain ones. Counting
# those would multiply every candidate by six and say nothing new.
SWEEP_SUFFIX = ("O2GyGSfpfast", "O2GyGSfpprecise", "O2GyGS",
                "O2OsGyGSfpfast", "OxGyGSfpfast", "O1GyGSfpfast")


def manifest():
    """-> (addresses already matched, symbols already claimed)."""
    addrs, syms = {}, set()
    for line in MANIFEST.read_text(encoding="utf-8").splitlines():
        s = line.split("#")[0].strip()
        if not s:
            continue
        f = s.split()
        if len(f) < 2:
            continue
        try:
            addrs[int(f[1], 16)] = f[0]
        except ValueError:
            continue
        for extra in f[2:]:
            if not extra.startswith("flags=") and extra != "-":
                syms.add(extra)
    return addrs, syms


def compiled_by_size():
    """-> {trimmed length: [(label, symbol)]} from build/ogg/*.obj."""
    out = defaultdict(list)
    n = 0
    for p in sorted(WORK.glob("*.obj")):
        if any(p.stem.endswith(s) for s in SWEEP_SUFFIX):
            continue
        parts = p.stem.split("_")
        if len(parts) < 3:
            continue
        lbl = "%s/%s/%s.c" % (parts[0], parts[1], "_".join(parts[2:]))
        if not (ROOT / VENDOR / lbl).exists():
            continue
        for sym, code, mask in coff_functions(p.read_bytes()):
            code, _m = trim_padding(code, mask)
            if len(code) >= 8:
                out[len(code)].append((lbl, sym))
                n += 1
    return out, n


def main(argv):
    if not WORK.exists() or not any(WORK.glob("*.obj")):
        print("No objects under %s." % WORK.relative_to(ROOT).as_posix())
        print("Run `python tools/oggmatch.py` first. REFUSING to print an")
        print("empty work list, which reads as 'nothing to find' when it")
        print("means 'nothing was measured'.")
        return 1

    have, claimed = manifest()
    by_size, ncompiled = compiled_by_size()
    print("%d compiled function(s) indexed by length, %d distinct length(s)"
          % (ncompiled, len(by_size)))

    jobs = []
    for addr, size in sorted(load_inventory()):
        if addr in have or not any(lo <= addr < hi for lo, hi in BANDS):
            continue
        seen = set()
        # can_shrink and can_extend move the boundary by whole words, so a
        # candidate one word either side is still worth trying.
        for d in (0, -4, 4):
            for lbl, sym in by_size.get(size + d, ()):
                # A symbol already claimed at another address cannot also be
                # this one. Dropping it removes a pair that is known false,
                # which is not the same as narrowing the search.
                if sym in claimed or (lbl, sym) in seen:
                    continue
                seen.add((lbl, sym))
                jobs.append((addr, size, lbl, sym))
    jobs.sort(key=lambda j: -j[1])
    unmatched = len(set(j[0] for j in jobs))
    print("%d pair(s) over %d unmatched function(s) in the two bands"
          % (len(jobs), unmatched))
    print("")

    won, tally = {}, {"match": 0, "differ": 0, "unmeasured": 0}
    for addr, size, lbl, sym in jobs:
        if addr in won:
            continue
        r = subprocess.run(
            [sys.executable, "tools/match.py", "%s/%s" % (VENDOR, lbl),
             "%08X" % addr, "--sym", sym, "--flags", FLAGS],
            cwd=str(ROOT), capture_output=True, text=True)
        verdict, why = classify_match(r.returncode,
                                      (r.stdout or "") + (r.stderr or ""))
        tally[verdict] += 1
        if verdict == "match":
            won[addr] = (size, lbl, sym)
            print("  MATCH        %08X %6d  %-24s %s" % (addr, size, lbl, sym))
        elif verdict == "unmeasured":
            print("  UNMEASURED   %08X %6d  %-24s %-24s %s"
                  % (addr, size, lbl, sym[:24], why[:44]))

    print("")
    print("%d pair(s) tested over %d unmatched function(s):"
          % (sum(tally.values()), unmatched))
    print("  match %d, differ %d, UNMEASURED %d"
          % (tally["match"], tally["differ"], tally["unmeasured"]))
    print("%s byte(s) in the matching ones. UNMEASURED is not `differ`:"
          % "{:,}".format(sum(s for s, _l, _y in won.values())))
    print("those pairs were never compared and say nothing either way.")

    if "--write" not in argv:
        print("")
        print("nothing written; pass --write to write %s"
              % OUT.relative_to(ROOT).as_posix())
        return 0

    lines = ["# address size file symbol -- paired by LENGTH and verified by",
             "# tools/match.py. tools/oggpair.py; NOT a masked identification.",
             "# tree: libogg-1.1.3 libvorbis-1.2.0"]
    for addr in sorted(won):
        size, lbl, sym = won[addr]
        lines.append("%08X %6d %-32s %s" % (addr, size, lbl, sym))
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("")
    print("wrote %s (%d site(s))" % (OUT.relative_to(ROOT).as_posix(),
                                     len(won)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
