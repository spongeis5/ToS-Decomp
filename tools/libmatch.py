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
from peimage import Image, load_functions

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


def main(argv):
    img = Image()
    pdata = dict(load_functions())

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
                code, mask = trim_padding(code, mask)
                if len(code) < 8:
                    stats["too_short"] += 1
                    continue
                if not (mask[0] and mask[1] and mask[2] and mask[3]):
                    stats["first_word_relocated"] += 1
                    continue
                if sum(mask) < min_bytes:
                    stats["too_masked"] += 1
                    per_lib[lib.name]["too_masked"] += 1
                    continue
                wanted[code[:4]].append(
                    (lib.name, objname, sym, code, sum(mask), unmasked_runs(mask)))
        print("  %-26s fns %6d  unusable %5d"
              % (lib.name, per_lib[lib.name]["lib_functions"],
                 per_lib[lib.name]["too_masked"]))

    indexable = sum(len(v) for v in wanted.values())
    print("\n  %d distinct first-word key(s) over %d indexable function(s)"
          % (len(wanted), indexable))

    print("\npass 2: scanning every aligned position in executable sections")
    matched = {}
    positions = 0
    for s in img.sections:
        if not (s["exec"] and s["initialized"]):
            continue
        # RVA == offset in the unpacked buffer; PointerToRawData is the stale
        # original file layout and reading through it walks the wrong bytes.
        start = s["va"] - img.base
        blob = img.data[start : start + (s["vsize"] or s["rawsz"])]
        n = len(blob) & ~3
        positions += n // 4
        hits_here = 0
        for off in range(0, n, 4):
            cands = wanted.get(blob[off : off + 4])
            if not cands:
                continue
            for libname, objname, sym, code, un, runs in cands:
                if off + len(code) > len(blob):
                    continue
                ok = True
                for a, b_ in runs:
                    if blob[off + a : off + b_] != code[a:b_]:
                        ok = False
                        break
                if ok:
                    va = s["va"] + off
                    prev = matched.get(va)
                    if prev is None or prev[4] < un:
                        matched[va] = (libname, objname, sym, len(code), un)
                    stats["matches"] += 1
                    hits_here += 1
                    break
        print("  %-10s %9d aligned position(s), %6d site(s) identified"
              % (s["name"], n // 4, hits_here))

    in_pdata = sum(1 for va in matched if va in pdata)
    print()
    print("library functions examined     : %6d" % stats["lib_functions"])
    print("  trimmed to under 8 bytes     : %6d" % stats["too_short"])
    print("  first word relocated         : %6d" % stats["first_word_relocated"])
    print("  too masked to judge (<%2d B)  : %6d" % (min_bytes, stats["too_masked"]))
    print("  indexable                    : %6d" % indexable)
    print("  aligned positions scanned    : %6d" % positions)
    print()
    print("DISTINCT IMAGE SITES IDENTIFIED: %6d" % len(matched))
    print("  with a .pdata row            : %6d" % in_pdata)
    print("  with NO .pdata row           : %6d" % (len(matched) - in_pdata))
    print()
    print(".pdata functions total         : %6d" % len(pdata))
    print(".pdata functions identified    : %6d  (%.1f%%)"
          % (in_pdata, 100.0 * in_pdata / len(pdata)))
    print(".pdata functions REMAINING     : %6d" % (len(pdata) - in_pdata))

    out = Path("build/lib_matches.txt")
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
