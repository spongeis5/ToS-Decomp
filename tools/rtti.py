"""Recover classes and vtables from MSVC RTTI.

Havok was compiled with RTTI on, so the image carries 329 type descriptors.
That is a way to attribute -- and NAME -- Havok's virtual functions without
having the Havok libraries, which is the one piece of middleware that has not
turned up.

The MSVC chain, walked backwards then forwards:

    TypeDescriptor  { void* vftable; void* spare; char name[]; }
        name is the ".?AV<class>@@" string, so the descriptor starts 8 bytes
        before it.

    RTTICompleteObjectLocator { sig; offset; cdOffset;
                                TypeDescriptor* pTypeDescriptor;   <- +12
                                ClassHierarchy* pClassDescriptor; }

    vtable[-1] -> the CompleteObjectLocator, so the vtable begins one dword
        after whatever points at the locator.

Every step is checked rather than assumed: a locator must have signature 0
and point back at the descriptor, and a vtable slot is only accepted while it
lands in an executable section.  Counts are reported with denominators at
each stage so a chain that breaks is visible instead of yielding silence.
"""

import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_functions

INVENTORY = Path("build/functions_all.txt")

NAME_RE = re.compile(rb"\.\?A[VU][A-Za-z0-9_@?$]{2,160}@@\x00")


def main():
    img = Image()
    base = img.base
    data = img.data
    pdata = dict(load_functions())
    # The union inventory (.pdata + Ghidra) is the population of real function
    # starts. A vtable slot is accepted only if it names one: "points into an
    # executable section" is too weak and walks off the end of one vtable into
    # the next, attributing the next class's methods to this one.
    inventory = {}
    for line in INVENTORY.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        inventory[int(f[0], 16)] = int(f[1])

    exec_ranges = [(s["va"], s["va"] + (s["vsize"] or s["rawsz"]))
                   for s in img.sections if s["exec"] and s["initialized"]]

    def is_code(va):
        return any(lo <= va < hi for lo, hi in exec_ranges)

    # --- index every aligned dword by value, once ---
    n = len(data) // 4
    words = struct.unpack_from(">%dI" % n, data, 0)
    where = defaultdict(list)
    for i, w in enumerate(words):
        if w >= base:
            where[w].append(base + i * 4)

    # --- 1. type descriptors ---
    descs = []
    for m in NAME_RE.finditer(data):
        name_va = base + m.start()
        td = name_va - 8
        descs.append((td, m.group(0)[:-1].decode("latin1")))
    print("type descriptor(s) found: %d" % len(descs))

    # --- 2. locators pointing at each descriptor ---
    stats = {"no_locator": 0, "locators": 0, "no_vtable": 0, "vtables": 0}
    classes = []
    for td, name in descs:
        locs = []
        for ref in where.get(td, ()):
            col = ref - 12
            off = img.offset(col)
            if off is None or off + 20 > len(data):
                continue
            sig = struct.unpack_from(">I", data, off)[0]
            if sig != 0:
                continue
            locs.append(col)
        if not locs:
            stats["no_locator"] += 1
            continue
        stats["locators"] += len(locs)

        vts = []
        for col in locs:
            for ref in where.get(col, ()):
                vts.append(ref + 4)
        if not vts:
            stats["no_vtable"] += 1
            continue
        stats["vtables"] += len(vts)
        classes.append((name, td, locs, vts))

    print("  with at least one locator : %d" % len(
        [c for c in classes] ) + " (%d descriptor(s) had none)" % stats["no_locator"])
    print("  locators                  : %d" % stats["locators"])
    print("  vtables                   : %d" % stats["vtables"])
    print("  locators with no vtable   : %d" % stats["no_vtable"])

    # --- 3. read the vtables ---
    fn_class = {}
    not_a_known_fn = [0]
    stopped_at_next = [0]
    slot_rows = []
    # Every vtable start, so a walk can stop where the next one begins rather
    # than running on through it and stealing the next class's methods.
    vt_starts = set()
    for _n, _td, _l, vts_ in classes:
        vt_starts.update(vts_)
    total_slots = 0
    in_pdata = 0
    for name, td, locs, vts in classes:
        for vt in vts:
            slot = 0
            while True:
                off = img.offset(vt + slot * 4)
                if off is None or off + 4 > len(data):
                    break
                fp = struct.unpack_from(">I", data, off)[0]
                if not is_code(fp):
                    break
                if slot and (vt + slot * 4) in vt_starts:
                    # The next vtable begins here; this one has ended.
                    stopped_at_next[0] += 1
                    break
                total_slots += 1
                if fp not in inventory:
                    not_a_known_fn[0] += 1
                if fp in pdata:
                    in_pdata += 1
                fn_class.setdefault(fp, set()).add(name)
                slot_rows.append((vt, slot, fp, name))
                slot += 1
                if slot > 512:
                    print("  CAP: vtable at %08X reached 512 slots" % vt)
                    break

    print()
    print("vtable slots read        : %d" % total_slots)
    print("  walks stopped at the next vtable : %d" % stopped_at_next[0])
    print("  slots whose target is NOT in the inventory (thunks?) : %d"
          % not_a_known_fn[0])
    print("  target is a .pdata fn  : %d  (%.1f%%)"
          % (in_pdata, 100.0 * in_pdata / max(total_slots, 1)))
    print("distinct functions named : %d" % len(fn_class))
    single = sum(1 for v in fn_class.values() if len(v) == 1)
    print("  belonging to exactly 1 class : %d" % single)
    print("  shared by several classes    : %d" % (len(fn_class) - single))

    out = Path("build/rtti_vtables.txt")
    with out.open("w") as f:
        f.write("# vtable slot function class\n")
        for vt, slot, fp, name in slot_rows:
            f.write("%08X %4d %08X %s\n" % (vt, slot, fp, name))
    out2 = Path("build/rtti_functions.txt")
    with out2.open("w") as f:
        f.write("# function classes\n")
        for fp in sorted(fn_class):
            f.write("%08X %s\n" % (fp, " | ".join(sorted(fn_class[fp]))))
    print("\nwrote %s and %s" % (out, out2))

    print()
    print("sample classes and their vtable sizes:")
    shown = 0
    for name, td, locs, vts in classes:
        if shown >= 12:
            break
        sizes = []
        for vt in vts:
            c = sum(1 for r in slot_rows if r[0] == vt)
            sizes.append(c)
        print("  %-52s %d vtable(s), slots %s"
              % (name[4:][:52], len(vts), sizes[:4]))
        shown += 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
