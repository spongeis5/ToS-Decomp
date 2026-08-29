"""Find good first targets for byte-matching.

A first match should be a LEAF -- a function that calls nothing -- because
otherwise the codegen depends on symbols that do not exist yet and a mismatch
cannot be attributed to the source you wrote.

Leaf-ness is decided statically here, with no dependence on Ghidra: a
function is a leaf if it contains no `bl` (opcode 18 with LK set), no
`bcl`/`bclrl`/`bcctrl`, i.e. nothing that sets the link register.

Candidates are restricted to functions with NO attribution -- not matched to
an XDK library, not referencing a middleware source path, not carrying a
Havok timer name -- so they are the title's own code.

    python tools/candidates.py
    python tools/candidates.py --max-size 200 --floats
"""

import bisect
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory, in_xdk

MIN_SIZE = 16   # smaller than this is a fragment, not a function


def classify(img, va, size):
    """(calls, has_float, has_undecodable, terminates) for one function."""
    off = img.offset(va)
    if off is None or off + size > len(img.data):
        return None
    n = size // 4
    words = struct.unpack_from(">%dI" % n, img.data, off)
    calls = 0
    floats = 0
    vmx = 0
    for w in words:
        op = w >> 26
        if op == 18 and (w & 1):                     # bl / bla
            calls += 1
        elif op == 16 and (w & 1):                   # bcl
            calls += 1
        elif op == 19:
            xo = (w >> 1) & 0x3FF
            if xo in (16, 528) and (w & 1):          # bclrl / bcctrl
                calls += 1
        if op in (48, 49, 50, 51, 52, 53, 54, 55, 59, 63):
            floats += 1
        if op in (4, 5, 6):                          # VMX / VMX128 space
            vmx += 1
    last = words[-1] if n else 0
    term = last in (0x4E800020, 0x4E800420) or (last >> 26) == 18
    return calls, floats, vmx, term


def main(argv):
    img = Image()
    funcs = load_inventory()
    sizes = dict(funcs)

    max_size = 400
    if "--max-size" in argv:
        max_size = int(argv[argv.index("--max-size") + 1])
    want_floats = "--floats" in argv

    attrib = {}
    p = Path("build/attribution.txt")
    if not p.exists():
        print("build/attribution.txt missing -- run tools/attribute.py first",
              file=sys.stderr)
        return 1
    for line in p.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split(None, 3)
        attrib[int(f[0], 16)] = f[2]

    # "unattributed" is NOT the same as "the title's own code": several of the
    # smallest unattributed leaves sit INSIDE the XDK block at 822F03E8..82523A1C
    # and are simply XDK functions that failed to byte-match. Filter them here
    # rather than leaving it to whoever reads the output.
    unattributed = [a for a, _ in funcs
                    if attrib.get(a) == "UNKNOWN" and not in_xdk(a)]
    inside = sum(1 for a, _ in funcs
                 if attrib.get(a) == "UNKNOWN" and in_xdk(a))
    print("%d function(s) total; %d unattributed, of which %d lie inside a known"
          % (len(funcs), len(unattributed) + inside, inside))
    print("XDK region and are excluded. %d candidate(s) remain."
          % len(unattributed))

    st = Counter()
    cands = []
    for va in unattributed:
        size = sizes[va]
        r = classify(img, va, size)
        if r is None:
            st["unreadable"] += 1
            continue
        calls, floats, vmx, term = r
        st["examined"] += 1
        if calls:
            continue
        st["leaf"] += 1
        if vmx:
            st["leaf_with_vmx"] += 1
        # A function must be big enough to be a function and must END like one.
        # Ghidra's half of the inventory contains 1- and 4-byte entries and
        # fragments that do not terminate; offering those as match targets
        # wastes a session on something that is not a function.
        if size < MIN_SIZE:
            st["too_small"] += 1
            continue
        if not term:
            st["no_terminator"] += 1
            continue
        if size <= max_size:
            st["leaf_small"] += 1
            if not want_floats or floats:
                cands.append((va, size, floats, vmx, term))

    print("  examined            %6d" % st["examined"])
    print("  LEAF (calls nothing)%6d  (%.1f%% of unattributed)"
          % (st["leaf"], 100.0 * st["leaf"] / max(st["examined"], 1)))
    print("    rejected, under %d bytes %6d" % (MIN_SIZE, st["too_small"]))
    print("    rejected, no terminator  %6d" % st["no_terminator"])
    print("    of which <= %d B and sound %6d" % (max_size, st["leaf_small"]))
    print("    using VMX (readable via tools/ppcdis) %6d" % st["leaf_with_vmx"])
    print()

    cands.sort(key=lambda c: c[1])
    print("best first targets (leaf, unattributed, smallest first%s):"
          % (", float-using" if want_floats else ""))
    print("  %-10s %6s %7s %5s %s" % ("address", "bytes", "floats", "vmx", "ends well"))
    for va, size, floats, vmx, term in cands[:30]:
        print("  %08X %6d %7d %5d %s"
              % (va, size, floats, vmx, "yes" if term else "NO"))

    out = Path("build/candidates.txt")
    with out.open("w") as f:
        f.write("# address size float_ops vmx_ops terminates\n")
        for va, size, floats, vmx, term in cands:
            f.write("%08X %6d %6d %5d %s\n" % (va, size, floats, vmx, term))
    print("\nwrote %s (%d candidate(s))" % (out, len(cands)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
