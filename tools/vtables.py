"""Recover the GAME'S OWN vtables, which RTTI cannot see.

    python tools/vtables.py              the census
    python tools/vtables.py 82005678     one vtable, slot by slot
    python tools/vtables.py --typeids    only those with a type-ID getter

`tools/rtti.py` names 311 vtables, and every one of them is Havok's: the
game's own translation units were compiled with RTTI OFF, so there are no
type descriptors for them and no names to read.  What the game has instead
is its own type system -- 194 twelve-byte virtual getters, each returning a
distinct 32-bit constant into r3, of which 192 appear in a vtable and only 2
are ever reached by a direct `bl`.  That population and Havok's do not
overlap at all: zero of the 194 are RTTI-named.  So the constant IS the
game's replacement for RTTI, and each getter marks one of the game's classes.

That makes the getters ANCHORS.  A vtable is a run of pointers into `.text`
sitting in `.rdata`, and the hard part is not finding the runs but knowing
where one vtable ends and the next begins, because adjacent vtables form a
single uninterrupted run.  The boundary evidence used here is that a
constructor must load its own vtable's address to store it into the object,
so a vtable start is an address FORMED IN CODE by a `lis`/`addi` pair.  Runs
are therefore split at exactly those addresses -- measured, not guessed.

VALIDATION, and the reason this file may be trusted at all: rtti.py already
knows 311 vtable starts by a completely independent route -- walking
TypeDescriptor -> CompleteObjectLocator -> vtable backwards through MSVC's
own structures.  This tool must rediscover those same 311 starts from
pointer runs and code references alone.  The agreement figure is printed
every run with its denominator, and `--check` exits non-zero if it drops
below the recorded baseline, because a boundary rule that has quietly
started splitting vtables in the wrong place would otherwise still produce a
confident-looking census.
"""

import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent
RTTI_VTABLES = ROOT / "build/rtti_vtables.txt"
OUT = ROOT / "build/vtables.txt"
WINDOW = 12          # lis/addi lookahead, same bound addrtaken.py uses

# The share of rtti.py's 311 vtable starts this reconstruction must still
# find. Recorded from a run that was checked by hand; `--check` fails below
# it so that a regression in the boundary rule is loud rather than silent.
BASELINE = 0.90


def code_referenced_addresses(img, lo, hi):
    """Every address in [lo,hi) formed by a lis/addi or lis/ori pair.

    Same pairing as addrtaken.py, and deliberately so: a second, subtly
    different implementation of this is how two tools come to disagree about
    the same image. Returns {address: [site, ...]}.
    """
    found = defaultdict(list)
    sites = bound_hit = scanned = 0
    for s in img.sections:
        if not (s["exec"] and s["initialized"]):
            continue
        off = s["va"] - img.base
        avail = len(img.data) - off
        size = min(s["vsize"] or s["rawsz"], s["rawsz"], max(avail, 0))
        n = size // 4
        if n <= 0:
            continue
        words = struct.unpack_from(">%dI" % n, img.data, off)
        scanned += n
        for i, w in enumerate(words):
            if (w >> 26) != 15 or ((w >> 16) & 0x1F) != 0:
                continue                       # not lis rD,imm
            sites += 1
            rD = (w >> 21) & 0x1F
            himm = w & 0xFFFF
            paired = False
            for j in range(i + 1, min(i + 1 + WINDOW, n)):
                w2 = words[j]
                op = w2 >> 26
                val = None
                if op == 14 and ((w2 >> 16) & 0x1F) == rD:          # addi
                    loimm = w2 & 0xFFFF
                    if loimm >= 0x8000:
                        loimm -= 0x10000
                    val = ((himm << 16) + loimm) & 0xFFFFFFFF
                elif op == 24 and ((w2 >> 21) & 0x1F) == rD:        # ori
                    val = ((himm << 16) | (w2 & 0xFFFF)) & 0xFFFFFFFF
                if val is not None:
                    paired = True
                    if lo <= val < hi and (val & 3) == 0:
                        found[val].append(s["va"] + i * 4)
                    break
                if ((w2 >> 21) & 0x1F) == rD and op in (14, 15, 24, 31):
                    paired = True               # clobbered before pairing
                    break
            if not paired:
                bound_hit += 1
    return found, scanned, sites, bound_hit


def pointer_runs(img, tlo, thi):
    """Maximal runs of aligned .rdata/.data words pointing into .text."""
    runs = []
    for s in img.sections:
        if s["exec"] or not s["initialized"]:
            continue
        if s["name"] not in (".rdata", ".data"):
            continue
        off = s["va"] - img.base
        avail = len(img.data) - off
        size = min(s["vsize"] or s["rawsz"], s["rawsz"], max(avail, 0))
        n = size // 4
        if n <= 0:
            continue
        words = struct.unpack_from(">%dI" % n, img.data, off)
        start = None
        for i, w in enumerate(words):
            ok = (tlo <= w < thi) and (w & 3) == 0
            if ok and start is None:
                start = i
            elif not ok and start is not None:
                if i - start >= 2:
                    runs.append((s["va"] + start * 4, i - start, s["name"]))
                start = None
        if start is not None and n - start >= 2:
            runs.append((s["va"] + start * 4, n - start, s["name"]))
    return runs


def split_runs(runs, refs):
    """Cut each run at every address a constructor forms in code."""
    out = []
    for va, count, sect in runs:
        cuts = [0]
        for k in range(1, count):
            if (va + k * 4) in refs:
                cuts.append(k)
        cuts.append(count)
        for a, b in zip(cuts, cuts[1:]):
            if b > a:
                out.append((va + a * 4, b - a, sect))
    return out


def rtti_starts():
    if not RTTI_VTABLES.exists():
        return None, {}
    starts, names = set(), {}
    for line in RTTI_VTABLES.read_text().splitlines():
        if line.startswith("#"):
            continue
        f = line.split()
        if len(f) >= 4:
            a = int(f[0], 16)
            starts.add(a)
            names[a] = f[3]
    return starts, names


def type_id_getters(img, inv):
    """The 12-byte `lis r3 / ori r3 / blr` functions, address -> constant."""
    out = {}
    for addr, size in inv:
        if size != 12:
            continue
        raw = img.read(addr, 12)
        if raw is None or len(raw) != 12:
            continue
        w = struct.unpack(">III", raw)
        if w[2] != 0x4E800020:
            continue
        if (w[0] & 0xFC1F0000) != 0x3C000000:
            continue
        if (w[1] & 0xFC000000) != 0x60000000:
            continue
        d, s, a = (w[0] >> 21) & 31, (w[1] >> 21) & 31, (w[1] >> 16) & 31
        if d == s == a == 3:
            out[addr] = ((w[0] & 0xFFFF) << 16) | (w[1] & 0xFFFF)
    return out


def classify(value):
    """Not every constant-returning virtual is a type ID. Say which is which.

    Measured over the 194 found in this image:

        2   HRESULT-shaped 0x8000xxxx -- E_FAIL and E_NOTIMPL. These are COM
            stubs on a QueryInterface-style interface, and one of them is
            shared by 89 vtables. Counting them as class identities would
            have put 89 unrelated classes in one family.
        6   0x00008000, 0x8100 ... 0x8500 -- an enum on a 0x100 stride, a
            different identity scheme from the hashes.
       11   four printable ASCII bytes: 'seuc', 'smrp', 'smht', 'sgms'...
            which read back as cues / prms / thms / sgms. FourCC chunk tags,
            and all eleven sit in one 12 KiB neighbourhood.
      175   high-entropy, no structure found -- the class hashes proper.
            Not any of crc32 (three variants), fnv1/1a, djb2 (two), sdbm or
            jenkins-one-at-a-time, in as-is/lower/upper, over the 102,694
            strings in the image: 0 hits of 194. The names were stripped, so
            a negative result here does not mean it is not a name hash.
    """
    if (value >> 16) == 0x8000:
        return "hresult"
    if value < 0x10000:
        return "enum"
    if all(0x20 <= ((value >> s) & 0xFF) < 0x7F for s in (24, 16, 8, 0)):
        return "fourcc"
    return "hash"


def build():
    img = Image()
    inv = sorted(load_inventory())
    starts = set(a for a, _n in inv)

    text = next(s for s in img.sections if s["name"] == ".text")
    tlo = text["va"]
    thi = text["va"] + (text["vsize"] or text["rawsz"])

    rlo = min(s["va"] for s in img.sections
              if s["name"] in (".rdata", ".data"))
    rhi = max(s["va"] + (s["vsize"] or s["rawsz"]) for s in img.sections
              if s["name"] in (".rdata", ".data"))

    refs, scanned, sites, bound_hit = code_referenced_addresses(img, rlo, rhi)
    runs = pointer_runs(img, tlo, thi)
    tables = split_runs(runs, refs)
    getters = type_id_getters(img, inv)
    return img, inv, starts, tables, runs, refs, getters, \
        (scanned, sites, bound_hit)


def manifest_addresses():
    """Addresses that already have a source, matched or attempted."""
    done = set()
    for name in ("manifest.txt", "attempts.txt"):
        p = ROOT / "src" / name
        if not p.exists():
            continue
        for line in p.read_text().splitlines():
            line = line.split("#")[0].strip()
            if not line:
                continue
            f = line.split()
            if len(f) >= 2:
                try:
                    done.add(int(f[1], 16))
                except ValueError:
                    pass
    return done


def rank(img, inv, slots, byaddr, tid_of, getters):
    """Which CLASS is the cheapest to finish?

    climb.py ranks one function by how much of it is already known;
    layout.py ranks by how much structure a match would pin. Neither can see
    a class, because a class is not a call-graph fact -- it is a vtable. A
    class whose methods are mostly small and mostly unwritten is a whole
    coherent unit obtainable in one sitting, and finishing one is worth more
    than the same number of unrelated functions: the methods share a `this`,
    so every field offset one of them reveals constrains all the others.

    Only tables carrying one of the game's own type IDs are ranked. The rest
    are Havok's, which rtti.py already names, or are not classes at all.
    """
    sizes = dict(inv)
    done = manifest_addresses()

    rows = []
    for va, ids in tid_of.items():
        # A class identity, not a COM stub or an inherited enum.
        kinds = set(classify(c) for _k, _w, c in ids)
        if "hash" not in kinds:
            continue
        ms = slots.get(va, [])
        uniq = []
        seen = set()
        for m in ms:
            if m not in seen:
                seen.add(m)
                uniq.append(m)
        known = [m for m in uniq if m in sizes]
        if not known:
            continue
        bytes_total = sum(sizes[m] for m in known)
        already = sum(1 for m in known if m in done)
        remaining = [m for m in known if m not in done]
        rem_bytes = sum(sizes[m] for m in remaining)
        med = sorted(sizes[m] for m in remaining)
        median = med[len(med) // 2] if med else 0
        rows.append((va, len(uniq), len(known), bytes_total, already,
                     len(remaining), rem_bytes, median,
                     "%08X" % ids[0][2]))

    # Cheapest first: few methods left, and small ones.
    rows.sort(key=lambda r: (r[6], r[5]))
    print("CLASSES OF THE GAME'S OWN, ranked by what finishing one costs")
    print("")
    print("  vtable    slots  known  done  left  left_bytes  median  typeid")
    for r in rows[:30]:
        print("  %08X %5d  %5d %5d %5d %11d %7d  %s"
              % (r[0], r[1], r[2], r[4], r[5], r[6], r[7], r[8]))
    print("")
    print("  %d class(es) with a hash-shaped type ID and at least one method"
          % len(rows))
    tot = sum(r[6] for r in rows)
    print("  %d byte(s) of method code across all of them, of which %d"
          % (sum(r[3] for r in rows), tot))
    print("  byte(s) are not yet written")
    print("")
    print("  `vtables.py <addr>` lists one class slot by slot.")
    return 0


def main(argv):
    img, inv, starts, tables, runs, refs, getters, scan = build()
    scanned, sites, bound_hit = scan

    byaddr = {va: (n, sect) for va, n, sect in tables}
    slots = {}
    for va, n, _s in tables:
        off = va - img.base
        slots[va] = list(struct.unpack_from(">%dI" % n, img.data, off))

    # Which tables carry one of the game's type-ID getters?
    tid_of = {}
    for va, ws in slots.items():
        for k, w in enumerate(ws):
            if w in getters:
                tid_of.setdefault(va, []).append((k, w, getters[w]))

    if "--rank" in argv:
        return rank(img, inv, slots, byaddr, tid_of, getters)

    if len(argv) > 1 and argv[1] not in ("--typeids", "--check", "--rank"):
        want = int(argv[1], 16)
        if want not in slots:
            print("%08X is not a reconstructed vtable start." % want)
            near = [v for v in sorted(slots) if abs(v - want) < 0x200]
            if near:
                print("Nearest reconstructed start(s): %s"
                      % ", ".join("%08X" % v for v in near))
            return 1
        n, sect = byaddr[want]
        print("vtable %08X in %s -- %d slot(s)" % (want, sect, n))
        _rs, rnames = rtti_starts()
        if want in rnames:
            print("  RTTI names this class %s" % rnames[want])
        for k, w in enumerate(slots[want]):
            note = ""
            if w in getters:
                note = "   <- TYPE ID 0x%08X" % getters[w]
            elif w not in starts:
                note = "   (not a known function start)"
            print("  [%3d] %08X%s" % (k, w, note))
        return 0

    rs, rnames = rtti_starts()
    lines = []
    lines.append("# vtable  slots  section  typeid_slot  typeid  rtti_name")
    for va, n, sect in sorted(tables):
        t = tid_of.get(va)
        ts = "%d" % t[0][0] if t else "-"
        tv = "%08X" % t[0][2] if t else "-"
        lines.append("%08X %4d %-7s %4s %8s %s"
                     % (va, n, sect, ts, tv, rnames.get(va, "-")))
    OUT.write_text("\n".join(lines) + "\n")

    print("scanned %d instruction word(s); %d lis site(s), %d hit the"
          % (scanned, sites, bound_hit))
    print("  %d-word lookahead bound without pairing -- a bound on this"
          % WINDOW)
    print("  scan, not a statement about the image.")
    print("")
    print("%d pointer run(s) in .rdata/.data" % len(runs))
    print("%d address(es) in .rdata/.data formed by code" % len(refs))
    print("%d vtable(s) after splitting runs at those addresses"
          % len(tables))
    print("")

    # A getter is PLACED once per vtable that carries it, and a derived class
    # that does not override the base's getter carries the base's. So
    # placements exceed getters, and the excess is not an error -- it is the
    # inheritance signal. Reporting one count as the other is how a census
    # comes to claim more instances of a thing than the thing has.
    withid = len(tid_of)
    placements = sum(len(v) for v in tid_of.values())
    distinct = len(set(w for v in tid_of.values() for _k, w, _c in v))
    print("THE GAME'S OWN CLASSES")
    print("  %d constant-returning virtual(s) exist, by shape:" % len(getters))
    kinds = defaultdict(int)
    for _a, v in getters.items():
        kinds[classify(v)] += 1
    for k in ("hash", "fourcc", "enum", "hresult"):
        print("      %-8s %3d" % (k, kinds.get(k, 0)))
    print("  Only the %d hash-shaped are class identities; see classify()."
          % kinds.get("hash", 0))
    print("  %d of the %d appear in a reconstructed vtable"
          % (distinct, len(getters)))
    print("  %d placement(s) across %d vtable(s) -- a getter appears once per"
          % (placements, withid))
    print("  class that does not override it, so placements > getters is the")
    print("  INHERITANCE, not a miscount.")
    if tid_of:
        sl = defaultdict(int)
        for v in tid_of.values():
            for k, _w, _c in v:
                sl[k] += 1
        top = sorted(sl.items(), key=lambda kv: -kv[1])
        print("  slot index of the getter, top 5 of %d distinct index(es):"
              % len(sl))
        for k, n in top[:5]:
            print("      slot %-4d %4d table(s)" % (k, n))
        tail = sum(n for _k, n in top[5:])
        print("      %d further placement(s) at %d other index(es)"
              % (tail, max(0, len(top) - 5)))
        print("  Slot %d dominates: that is where this engine puts its type"
              % top[0][0])
        print("  getter. A placement at a far higher index is a secondary")
        print("  vtable of a multiply-inherited class, or a run this tool")
        print("  did not manage to split -- not a different convention.")

        # Families: one getter shared by several vtables means those classes
        # inherit it from a common base. This is an inheritance graph read
        # out of data alone, with no RTTI and no names.
        fam = defaultdict(set)
        for va, v in tid_of.items():
            for _k, w, c in v:
                fam[(w, c)].add(va)      # a SET: one vtable can hold the
                                         # same getter at two slots, and
                                         # counting that twice would inflate
                                         # a family to six members when it
                                         # has three.
        shared = {k: v for k, v in fam.items() if len(v) > 1}
        print("")
        print("  %d getter(s) appear in more than one vtable -- each is a"
              % len(shared))
        print("  base class whose derived classes did not override it:")
        for (w, c), vs in sorted(shared.items(),
                                 key=lambda kv: -len(kv[1]))[:10]:
            print("      %08X returns %08X, in %d vtable(s): %s"
                  % (w, c, len(vs),
                     " ".join("%08X" % a for a in sorted(vs)[:6])
                     + (" ..." if len(vs) > 6 else "")))

    print("")
    if rs is None:
        print("build/rtti_vtables.txt is missing -- cannot validate. Run")
        print("tools/rtti.py. Refusing to present the census above as")
        print("checked when the one independent check available did not run.")
        return 1
    found = sum(1 for a in rs if a in byaddr)
    frac = found / float(len(rs)) if rs else 0.0
    print("VALIDATION against rtti.py, which knows %d vtable start(s) by an"
          % len(rs))
    print("independent route:")
    print("  rediscovered %d of %d  (%.1f%%)" % (found, len(rs), 100.0 * frac))
    missed = sorted(a for a in rs if a not in byaddr)
    if missed:
        print("  missed, first 10: %s"
              % ", ".join("%08X" % a for a in missed[:10]))
        inside = sum(1 for a in missed
                     if any(v < a < v + n * 4 for v, n, _s in tables))
        print("  of the %d missed, %d fall INSIDE a reconstructed table --"
              % (len(missed), inside))
        print("  that is a boundary the code-reference rule did not see,")
        print("  not a table this tool failed to find.")
    print("")
    print("wrote %s" % OUT)

    if "--check" in argv and frac < BASELINE:
        print("")
        print("FAIL: %.1f%% is below the %.0f%% baseline."
              % (100.0 * frac, 100.0 * BASELINE))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
