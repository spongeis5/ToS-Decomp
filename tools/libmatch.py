"""Match XDK library code against the retail image.

A byte-matching decomp does not need to reconstruct code that came out of a
library -- that code links from the original `.lib`.  This finds it, so the
project's real scope is a measured number rather than "21,238 functions".

Two things had to be got right, and each produced 0 matches when wrong:

1. **Relocations.** Library object code carries placeholder bytes where the
   linker patches addresses, so a raw comparison finds nothing.  Every
   instruction word a relocation touches is MASKED and the match is judged
   on the rest.  Every reported match states how many bytes were actually
   compared, because masking costs discriminating power.

2. **`.pdata` is the WRONG search space.**  It covers 92.5% of executable
   bytes, and what is missing from it is largely leaf functions that need no
   unwind data -- which is most of the CRT.  Indexing candidates on `.pdata`
   starts found 0 while a whole-image masked scan found `strncmp`, `longjmp`
   and `memchr` immediately.  Candidates are therefore every 4-byte-aligned
   position in an executable section.

    python tools/libmatch.py libcMT.lib
    python tools/libmatch.py --all
    python tools/libmatch.py --all --min-bytes 24
"""

import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_functions, load_inventory

LIBDIR = Path("SDKFiles/xdk/XDK/lib/xbox")
MIN_UNMASKED = 16
IMAGE_FILE_MACHINE_POWERPCBE = 0x01F2
NOP = bytes([0x60, 0x00, 0x00, 0x00])
ZERO4 = bytes(4)
ARCH_MAGIC = bytes([0x21, 0x3C, 0x61, 0x72, 0x63, 0x68, 0x3E, 0x0A])  # !<arch>\n
MEMBER_END = bytes([0x60, 0x0A])


def archive_members(data):
    """Yield (name, blob) for each real object in a COFF archive."""
    if data[:8] != ARCH_MAGIC:
        raise ValueError("not a COFF archive")
    pos, longnames, idx = 8, b"", 0
    while pos + 60 <= len(data):
        hdr = data[pos : pos + 60]
        if hdr[58:60] != MEMBER_END:
            break
        name = hdr[0:16].decode("latin1").rstrip()
        try:
            size = int(hdr[48:58].decode("latin1").strip())
        except ValueError:
            break
        body = data[pos + 60 : pos + 60 + size]
        pos += 60 + size
        if size & 1:
            pos += 1
        idx += 1
        if name.startswith("/") and name[1:].strip() == "":
            continue                                  # linker symbol member
        if name == "//":
            longnames = body
            continue
        if name.startswith("/"):
            try:
                off = int(name[1:])
                end = longnames.index(bytes(1), off)
                name = longnames[off:end].decode("latin1")
            except (ValueError, IndexError):
                name = "<member%d>" % idx
        yield name.rstrip("/"), body


def coff_functions(blob):
    """[(symbol, code, mask)] for each function in a COFF object.
    mask: 1 = byte is meaningful, 0 = relocated, ignore it."""
    if len(blob) < 20:
        return []
    mach, nsec, _ts, psym, nsym, osz, _ch = struct.unpack_from("<HHIIIHH", blob, 0)
    if mach != IMAGE_FILE_MACHINE_POWERPCBE or nsym == 0:
        return []

    sh = 20 + osz
    secs = []
    for i in range(nsec):
        b = sh + i * 40
        if b + 40 > len(blob):
            return []
        _vsize, _va, rawsz, rawptr = struct.unpack_from("<IIII", blob, b + 8)
        relptr = struct.unpack_from("<I", blob, b + 24)[0]
        nrel = struct.unpack_from("<H", blob, b + 32)[0]
        chars = struct.unpack_from("<I", blob, b + 36)[0]
        secs.append(dict(idx=i + 1, size=rawsz, ptr=rawptr, relptr=relptr,
                         nrel=nrel, is_code=bool(chars & 0x20000000)))

    strtab = psym + nsym * 18
    syms, i = [], 0
    while i < nsym:
        o = psym + i * 18
        if o + 18 > len(blob):
            break
        raw = blob[o : o + 8]
        if raw[:4] == ZERO4:
            off = struct.unpack_from("<I", blob, o + 4)[0]
            e = blob.find(bytes(1), strtab + off)
            name = blob[strtab + off : e].decode("latin1") if e > 0 else "?"
        else:
            name = raw.rstrip(bytes(1)).decode("latin1")
        value, secnum, _typ, cls, naux = struct.unpack_from("<IhHBB", blob, o + 8)
        syms.append(dict(name=name, value=value, sec=secnum, cls=cls))
        i += 1 + naux

    out = []
    for s in secs:
        if not s["is_code"] or not s["size"] or not s["ptr"]:
            continue
        code = blob[s["ptr"] : s["ptr"] + s["size"]]
        if len(code) != s["size"]:
            continue
        mask = bytearray(b"\x01" * len(code))
        for r in range(s["nrel"]):
            ro = s["relptr"] + r * 10
            if ro + 10 > len(blob):
                break
            w = struct.unpack_from("<I", blob, ro)[0] & ~3
            for k in range(w, min(w + 4, len(mask))):
                mask[k] = 0
        fs = sorted([y for y in syms
                     if y["sec"] == s["idx"] and y["cls"] in (2, 3)
                     and not y["name"].startswith(".")],
                    key=lambda y: y["value"])
        for n, f in enumerate(fs):
            a = f["value"]
            b_ = fs[n + 1]["value"] if n + 1 < len(fs) else len(code)
            if b_ > a:
                out.append((f["name"], code[a:b_], bytes(mask[a:b_])))
    return out


def trim_padding(code, mask):
    """Drop trailing COMDAT alignment padding: zero words or PPC nops."""
    n = len(code) & ~3
    while n >= 4:
        w = code[n - 4 : n]
        if w == NOP or w == ZERO4:
            n -= 4
        else:
            break
    return code[:n], mask[:n]


def unmasked_runs(mask):
    """Contiguous spans of meaningful bytes, so comparison uses slice
    equality at C speed rather than a Python byte loop."""
    runs, start = [], None
    for i, m in enumerate(mask):
        if m and start is None:
            start = i
        elif not m and start is not None:
            runs.append((start, i))
            start = None
    if start is not None:
        runs.append((start, len(mask)))
    return runs


def pick_function(fns, sym):
    """The one function a manifest row names. -> (entry, None) or (None, why).

    `fns` is [(name, code, anything)] from coff_functions or
    functions_with_relocs; only the name is examined.

    THE ORDER OF THE THREE TESTS IS LORE, and the exact-match step in the
    middle is the one that matters. C symbols are not mangled, so `?name@@`
    never matches them, and the substring test alone is wrong for any name
    that is a prefix of another:

        "vorbis_book_decode" in "vorbis_book_decodev_add"   -> True

    `matched_table.compiled_size` had the mangled test and the substring test
    with no exact test between them, then took `max` by length of whatever
    survived. So `vorbis_book_decode` -- 100 bytes -- was measured as 572,
    the length of `vorbis_book_decodev_add`. Four rows were wrong that way and
    the reported total was inflated by 488 bytes, which is what made build.py
    and report.py disagree.

    REFUSING is the other half. build.py has always refused an ambiguous row
    ("picking the largest silently builds the wrong function the moment a
    translation unit grows a second one") and match.py now does too. This is
    that rule in one place, so the fourth tool cannot re-derive it wrongly.
    """
    if sym:
        for cand in ([f for f in fns if ("?" + sym + "@@") in f[0]],
                     [f for f in fns if f[0] == sym],
                     [f for f in fns if sym in f[0]]):
            if len(cand) == 1:
                return cand[0], None
            if len(cand) > 1:
                return None, ("%r matches %d functions: %s"
                              % (sym, len(cand),
                                 ", ".join(f[0][:40] for f in cand[:4])))
        return None, "%r matches no function in the object" % sym
    if len(fns) == 1:
        return fns[0], None
    if not fns:
        return None, "no function in the object"
    return None, ("%d functions in the object and the row names none: %s"
                  % (len(fns), ", ".join(f[0][:40] for f in fns[:4])))


def indexable(code, mask, min_bytes=MIN_UNMASKED):
    """-> (key, (code, unmasked, runs)) or (None, why not).

    The three refusals are what make a masked scan mean anything: a function
    trimmed to nothing, one whose first word is relocated (so it has no
    stable key), and one so masked that agreement would not be evidence.
    Callers prepend their own labels to the returned triple; `scan` reads
    only its last three elements.
    """
    code, mask = trim_padding(code, mask)
    if len(code) < 8:
        return None, "too_short"
    if not (mask[0] and mask[1] and mask[2] and mask[3]):
        return None, "first_word_relocated"
    if sum(mask) < min_bytes:
        return None, "too_masked"
    return code[:4], (code, sum(mask), unmasked_runs(mask))


def scan(img, wanted, report=None):
    """Every aligned position in every executable section. -> {va: entry}

    `wanted` maps a four-byte key to a list of entries whose LAST three
    elements are (code, unmasked byte count, unmasked runs); anything before
    them is carried through to the result untouched, so a caller can label
    entries however it likes.

    Factored out of main() so tools/oggmatch.py can identify compiled
    third-party source the same way rather than growing a second comparator.
    A masked scan is an IDENTIFICATION, not a verification: it says these
    bytes came from this code, and deliberately does not use match.py's
    can_shrink/can_extend, which answer whether a claimed match is exact.
    """
    matched = {}
    positions = 0
    for s in img.sections:
        if not (s["exec"] and s["initialized"]):
            continue
        # RVA == offset in the unpacked buffer; PointerToRawData is the stale
        # original file layout and reading through it walks the wrong bytes.
        start = s["va"] - img.base
        blob = img.data[start:start + (s["vsize"] or s["rawsz"])]
        n = len(blob) & ~3
        positions += n // 4
        hits_here = 0
        for off in range(0, n, 4):
            cands = wanted.get(blob[off:off + 4])
            if not cands:
                continue
            for entry in cands:
                code, un, runs = entry[-3], entry[-2], entry[-1]
                if off + len(code) > len(blob):
                    continue
                ok = True
                for a, b_ in runs:
                    if blob[off + a:off + b_] != code[a:b_]:
                        ok = False
                        break
                if ok:
                    va = s["va"] + off
                    prev = matched.get(va)
                    if prev is None or prev[-1] < un:
                        matched[va] = entry[:-3] + (len(code), un)
                    hits_here += 1
                    break
        if report is not None:
            report(s["name"], n // 4, hits_here)
    return matched, positions


def main(argv):
    img = Image()
    # The SCAN is over every aligned position and does not depend on either
    # inventory -- but the denominators reported below do. .pdata alone is
    # ~18% short, so reporting against it understates coverage and disagrees
    # with attribute.py, which uses the union.
    pdata = dict(load_functions())
    inventory = dict(load_inventory())

    min_bytes = MIN_UNMASKED
    if "--min-bytes" in argv:
        min_bytes = int(argv[argv.index("--min-bytes") + 1])

    if "--all" in argv:
        libs = sorted(LIBDIR.glob("*.lib"))
    else:
        names = [a for a in argv[1:] if not a.startswith("--") and not a.isdigit()]
        libs = [LIBDIR / n for n in names]
    if not libs:
        print(__doc__)
        return 1

    print("pass 1: reading libraries")
    wanted = defaultdict(list)
    stats = Counter()
    per_lib = defaultdict(Counter)

    for lib in libs:
        if not lib.exists():
            print("  %s: NOT FOUND" % lib.name, file=sys.stderr)
            continue
        try:
            members = list(archive_members(lib.read_bytes()))
        except (ValueError, MemoryError) as e:
            print("  %-24s SKIPPED: %s" % (lib.name, e), file=sys.stderr)
            continue
        for objname, blob in members:
            try:
                fs = coff_functions(blob)
            except Exception:
                per_lib[lib.name]["obj_unparsed"] += 1
                continue
            for sym, code, mask in fs:
                stats["lib_functions"] += 1
                per_lib[lib.name]["lib_functions"] += 1
                key, ent = indexable(code, mask, min_bytes)
                if key is None:
                    stats[ent] += 1
                    if ent == "too_masked":
                        per_lib[lib.name]["too_masked"] += 1
                    continue
                wanted[key].append((lib.name, objname, sym) + ent)
        print("  %-26s fns %6d  unusable %5d"
              % (lib.name, per_lib[lib.name]["lib_functions"],
                 per_lib[lib.name]["too_masked"]))

    n_indexable = sum(len(v) for v in wanted.values())
    print("\n  %d distinct first-word key(s) over %d indexable function(s)"
          % (len(wanted), n_indexable))

    print("\npass 2: scanning every aligned position in executable sections")
    matched, positions = scan(
        img, wanted,
        report=lambda name, n, hits: print(
            "  %-10s %9d aligned position(s), %6d site(s) identified"
            % (name, n, hits)))

    in_pdata = sum(1 for va in matched if va in pdata)
    in_inv = sum(1 for va in matched if va in inventory)
    print()
    print("library functions examined     : %6d" % stats["lib_functions"])
    print("  trimmed to under 8 bytes     : %6d" % stats["too_short"])
    print("  first word relocated         : %6d" % stats["first_word_relocated"])
    print("  too masked to judge (<%2d B)  : %6d" % (min_bytes, stats["too_masked"]))
    print("  indexable                    : %6d" % n_indexable)
    print("  aligned positions scanned    : %6d" % positions)
    print()
    print("DISTINCT IMAGE SITES IDENTIFIED: %6d" % len(matched))
    print("  at a known function start    : %6d" % in_inv)
    print("    of those, with a .pdata row: %6d" % in_pdata)
    print("  not a known function start   : %6d" % (len(matched) - in_inv))
    print()
    print("FUNCTION INVENTORY (.pdata + Ghidra)")
    print("  total                        : %6d" % len(inventory))
    print("  identified as XDK library    : %6d  (%.1f%%)"
          % (in_inv, 100.0 * in_inv / len(inventory)))
    print("  REMAINING                    : %6d" % (len(inventory) - in_inv))
    print()
    print("  (.pdata alone would say %d of %d = %.1f%%; it is ~18%% short and"
          % (in_pdata, len(pdata), 100.0 * in_pdata / len(pdata)))
    print("   disagrees with attribute.py, which uses the union.)")

    # Refuse to replace a FULLER result with a narrower one.
    #
    # `libmatch.py libcMT.lib` for a quick check silently overwrote the
    # 62-library output that attribute.py reads, turning 6,332 attributed
    # functions into 287. Nothing errored; the next attribution run would
    # simply have been wrong. A partial run is a legitimate thing to want, so
    # this refuses the WRITE rather than the run, and --force is on the record.
    out = Path("build/lib_matches.txt")
    if out.exists() and "--force" not in argv:
        prev = sum(1 for l in out.read_text().splitlines()
                   if l.strip() and not l.startswith("#"))
        if prev > len(matched):
            print()
            print("REFUSING TO WRITE %s" % out)
            print("  it holds %d site(s); this run found %d." % (prev, len(matched)))
            print("  Overwriting would narrow the input attribute.py reads.")
            print("  Re-run over all libraries, or pass --force if you mean it.")
            print()
            print("  (results above are still valid for the libraries given)")
            return 1

    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w") as f:
        f.write("# va size lib object symbol unmasked_bytes in_pdata\n")
        for va in sorted(matched):
            libn, obj, sym, size, un = matched[va]
            f.write("%08X %6d %s %s %s %d %s\n"
                    % (va, size, libn, obj, sym, un, va in pdata))
    print("\nwrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
