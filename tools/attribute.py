"""Consolidate every attribution signal into one scope picture.

A byte-matching decomp only has to reconstruct code nobody else wrote.  Four
independent signals say "this function is not the game's own":

  lib      matched byte-for-byte against an XDK library object
  srcpath  the function forms the address of a middleware source path
           (FMOD, ogg/vorbis, Havok) -- an assert or log inside that file
  havok    the function pushes a Havok monitor-stream timer name
  game     the function pushes a `Ttz`-prefixed name, which is the title's own

Signals can overlap; a function is counted once, and the precedence is
stated rather than left implicit.  Every figure carries its denominator, and
the remainder is what the project actually has to decompile.

Middleware attribution by a SINGLE reference is weaker evidence than a byte
match, so the two are reported separately and never summed into one
confident number.
"""

import bisect
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_functions

INVENTORY = Path("build/functions_all.txt")

TEXT_BYTES = 8467964

FAMILY = [
    ("fmod", re.compile(r"fmod|\\src\\fmod", re.I)),
    ("ogg_vorbis", re.compile(r"ogg_vorbis|vorbis|\bogg\b", re.I)),
    ("havok", re.compile(r"havok|\bhk[a-z]", re.I)),
    ("sfx", re.compile(r"\\lib\\sfx", re.I)),
    ("game", re.compile(r"branches[\\/]sb09", re.I)),
]


def family_of(path):
    for name, rx in FAMILY:
        if rx.search(path):
            return name
    return "other_middleware"


def main():
    img = Image()
    # The UNION inventory (.pdata + Ghidra), not .pdata alone: .pdata misses
    # leaf functions with no unwind row, and using it as the denominator
    # understated the population by ~18%.
    funcs = []
    for line in INVENTORY.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        funcs.append((int(f[0], 16), int(f[1])))
    funcs.sort()
    starts = [a for a, _ in funcs]
    sizes = dict(funcs)

    def owner(a):
        i = bisect.bisect_right(starts, a) - 1
        if i >= 0 and starts[i] <= a < starts[i] + sizes[starts[i]]:
            return starts[i]
        return None

    attrib = {}          # function -> (signal, detail)
    def claim(fn, signal, detail):
        if fn in attrib:
            return False
        attrib[fn] = (signal, detail)
        return True

    # 1. XDK library byte matches -- strongest evidence
    lib_rows = 0
    p = Path("build/lib_matches.txt")
    if p.exists():
        for line in p.read_text().splitlines():
            if line.startswith("#") or not line.strip():
                continue
            f = line.split()
            va = int(f[0], 16)
            if va in sizes:
                lib_rows += 1
                claim(va, "lib", f[2])

    # 2. middleware source-path references
    src_claims = Counter()
    p = Path("build/source_files.txt")
    if p.exists():
        for line in p.read_text().splitlines():
            if not line.strip():
                continue
            parts = line.split("\t")
            path = parts[0]
            fam = family_of(path)
            if fam == "game":
                continue                     # the game's own registrations
            for a in parts[2].split():
                fn = owner(int(a, 16))
                if fn is not None and claim(fn, "srcpath", fam):
                    src_claims[fam] += 1

    # 3. Havok / game profiler names
    prof = Counter()
    p = Path("build/profiler_names.txt")
    if p.exists():
        for line in p.read_text().splitlines():
            if line.startswith("#") or not line.strip():
                continue
            f = line.split(None, 2)
            fn = int(f[0], 16)
            names = f[2].strip().split(" | ")
            is_game = any(n.startswith("Ttz") for n in names)
            sig = "game_profiled" if is_game else "havok"
            if claim(fn, sig, names[0]):
                prof[sig] += 1

    # 4. Havok classes recovered from MSVC RTTI vtables
    rtti = 0
    p = Path("build/rtti_functions.txt")
    if p.exists():
        for line in p.read_text().splitlines():
            if line.startswith("#") or not line.strip():
                continue
            f = line.split(None, 1)
            fn = int(f[0], 16)
            if fn in sizes and claim(fn, "rtti_havok", f[1].strip().split(" | ")[0]):
                rtti += 1
    print("  RTTI vtable attributions: %d" % rtti)
    print()

    by_sig = Counter(v[0] for v in attrib.values())
    bytes_by_sig = Counter()
    for fn, (sig, _d) in attrib.items():
        bytes_by_sig[sig] += sizes[fn]

    total_fn = len(funcs)
    known_fn = len(attrib)
    known_b = sum(bytes_by_sig.values())

    print("%d function(s), %d byte(s) of .text\n" % (total_fn, TEXT_BYTES))
    print("  %-16s %7s  %11s  %s" % ("signal", "fns", "bytes", "share of .text"))
    for sig in ("lib", "srcpath", "havok", "rtti_havok", "game_profiled"):
        if by_sig[sig]:
            print("  %-16s %7d  %11d  %5.1f%%"
                  % (sig, by_sig[sig], bytes_by_sig[sig],
                     100.0 * bytes_by_sig[sig] / TEXT_BYTES))
    print("  %-16s %7d  %11d  %5.1f%%"
          % ("TOTAL known", known_fn, known_b, 100.0 * known_b / TEXT_BYTES))
    print("  %-16s %7d  %11d  %5.1f%%"
          % ("REMAINING", total_fn - known_fn, TEXT_BYTES - known_b,
             100.0 * (TEXT_BYTES - known_b) / TEXT_BYTES))
    print()
    print("  middleware attributed by source path, by family:")
    for k, v in src_claims.most_common():
        print("     %-20s %5d function(s)" % (k, v))

    print()
    print("  NOTE: 'lib' is a byte-for-byte match and is strong evidence.")
    print("  'srcpath' and 'havok' are attribution by a reference the function")
    print("  makes, which is weaker -- a game function could log a middleware")
    print("  path. They are listed separately and deliberately not merged.")

    # contiguity: does middleware cluster into regions the linker grouped?
    print()
    tagged = sorted((fn, attrib[fn][0]) for fn in attrib)
    runs = []
    cur_sig, cur_lo, cur_hi, cur_n = None, None, None, 0
    for fn, sig in tagged:
        if sig != cur_sig or (cur_hi is not None and fn - cur_hi > 0x2000):
            if cur_sig and cur_n >= 20:
                runs.append((cur_lo, cur_hi, cur_sig, cur_n))
            cur_sig, cur_lo, cur_n = sig, fn, 0
        cur_hi = fn + sizes[fn]
        cur_n += 1
    if cur_sig and cur_n >= 20:
        runs.append((cur_lo, cur_hi, cur_sig, cur_n))
    runs.sort(key=lambda r: -(r[1] - r[0]))
    print("  largest contiguous attributed regions (>=20 functions):")
    for lo, hi, sig, n in runs[:14]:
        print("     %08X..%08X  %8d B  %5d fn  %s" % (lo, hi, hi - lo, n, sig))

    out = Path("build/attribution.txt")
    with out.open("w") as f:
        f.write("# address size signal detail\n")
        for fn in sorted(sizes):
            sig, det = attrib.get(fn, ("UNKNOWN", "-"))
            f.write("%08X %7d %-14s %s\n" % (fn, sizes[fn], sig, det))
    print("\nwrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
