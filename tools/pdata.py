"""Walk the .pdata exception directory: the compiler's own function table.

Without a PDB this is the best function inventory the image has -- it is
emitted by the compiler, not recovered by a heuristic.

The 8-byte entry's second dword is a bitfield, and which end the fields are
allocated from depends on the endianness the struct was compiled under.
Rather than pick one from memory, this decodes BOTH ways and marks each
against properties that a correct decode must have and a wrong one will
fail:

  * every function begins inside .text
  * every length is a non-zero multiple of 4
  * begin + length does not run past the next function's begin
  * the covered bytes do not exceed the size of .text

A guess that decodes into something plausible is this problem's classic
failure; two arms marked against the same properties cannot both look right.
"""

import struct
import sys
from collections import Counter
from pathlib import Path

IMAGE = Path("build/default.pe.exe")


def load_sections(data):
    o = struct.unpack_from("<I", data, 0x3C)[0]
    if data[o : o + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    nsec, optsz = struct.unpack_from("<H", data, o + 6)[0], struct.unpack_from("<H", data, o + 20)[0]
    base = struct.unpack_from("<I", data, o + 24 + 28)[0]
    sh = o + 24 + optsz
    secs = []
    for i in range(nsec):
        b = sh + i * 40
        name = data[b : b + 8].rstrip(b"\0").decode("latin1")
        vsize, va, rawsz, rawptr = struct.unpack_from("<IIII", data, b + 8)
        chars = struct.unpack_from("<I", data, b + 36)[0]
        secs.append(dict(name=name, vsize=vsize, va=va, rawsz=rawsz,
                         rawptr=rawptr, chars=chars))
    return base, secs


def sec_by_name(secs, name):
    for s in secs:
        if s["name"] == name:
            return s
    raise KeyError(name)


def decode(raw_entries, arm):
    """arm 'lsb': prolog=bits0-7, len=bits8-29.
       arm 'msb': prolog=bits24-31, len=bits2-23."""
    out = []
    for begin, d in raw_entries:
        if arm == "lsb":
            prolog = d & 0xFF
            length = (d >> 8) & 0x3FFFFF
            flag32 = (d >> 30) & 1
            exc = (d >> 31) & 1
        else:
            prolog = (d >> 24) & 0xFF
            length = (d >> 2) & 0x3FFFFF
            flag32 = (d >> 1) & 1
            exc = d & 1
        out.append((begin, prolog, length, flag32, exc))
    return out


def mark(entries, exec_ranges, unit):
    """Score a decode against every EXECUTABLE section, not just .text --
    a function may legitimately live in any of them, and marking against
    .text alone measures the wrong population.
    unit is 4 if length counts instructions, 1 if bytes."""
    n = len(entries)
    in_exec = zero_len = misaligned = overlap = past_end = 0
    covered = 0
    exec_bytes = sum(hi - lo for lo, hi, _ in exec_ranges)
    for i, (begin, _pro, length, _f, _e) in enumerate(entries):
        size = length * unit
        home = None
        for lo, hi, name in exec_ranges:
            if lo <= begin < hi:
                home = (lo, hi, name)
                break
        if home:
            in_exec += 1
            if begin + size > home[1]:
                past_end += 1
        else:
            past_end += 1
        if size == 0:
            zero_len += 1
        if size % 4:
            misaligned += 1
        if i + 1 < n and begin + size > entries[i + 1][0]:
            overlap += 1
        covered += size
    return dict(n=n, in_exec=in_exec, zero_len=zero_len, misaligned=misaligned,
                overlap=overlap, past_end=past_end, covered=covered,
                exec_bytes=exec_bytes)


def main(argv):
    if not IMAGE.exists():
        print(f"{IMAGE} not found -- run tools/xex.py first", file=sys.stderr)
        return 1
    data = IMAGE.read_bytes()
    base, secs = load_sections(data)
    pdata = sec_by_name(secs, ".pdata")
    text = sec_by_name(secs, ".text")

    # IMAGE_SCN_MEM_EXECUTE = 0x20000000
    exec_ranges = []
    for s in secs:
        if s["chars"] & 0x20000000:
            lo = base + s["va"]
            exec_ranges.append((lo, lo + s["vsize"], s["name"]))
    exec_ranges.sort()
    text_lo = base + text["va"]
    text_hi = text_lo + text["vsize"]

    n = pdata["vsize"] // 8
    if pdata["vsize"] % 8:
        raise ValueError(
            f".pdata is {pdata['vsize']} bytes, not a multiple of the 8-byte "
            "entry size; the entry format assumed here is wrong"
        )

    # This tool is the BOOTSTRAP -- it runs before build/functions_all.txt
    # exists -- so it parses the section table itself rather than going
    # through peimage. That means it must be explicit about the mapping.
    #
    # The unpacked XEX is a MEMORY image: RVA == offset. .pdata is one of the
    # three sections whose PointerToRawData happens to equal its RVA, which is
    # the only reason reading through rawptr ever worked here. Assert it, so a
    # future change cannot silently start reading the wrong bytes -- that
    # exact defect cost most of a session (FINDINGS.md section 8).
    rva = base + pdata["va"] - base          # .pdata RVA
    if pdata["rawptr"] != rva:
        raise ValueError(
            ".pdata PointerToRawData (%08X) != its RVA (%08X). The unpacked "
            "image is a memory image, so the RVA is authoritative; this tool "
            "assumed they coincide." % (pdata["rawptr"], rva))
    off = rva
    raw = []
    for i in range(n):
        begin, d = struct.unpack_from(">II", data, off + i * 8)
        raw.append((begin, d))

    print(f".pdata  {pdata['vsize']:,} byte(s) at {base + pdata['va']:08X} "
          f"= {n:,} entries of 8 bytes")
    print(f".text   {text['vsize']:,} byte(s), {text_lo:08X}..{text_hi:08X}")
    print()

    print("  raw first 4 entries:")
    for begin, d in raw[:4]:
        print(f"    begin {begin:08X}  data {d:08X}")
    print()

    best = None
    for arm in ("lsb", "msb"):
        ent = decode(raw, arm)
        for unit, unit_name in ((4, "instructions"), (1, "bytes")):
            m = mark(ent, exec_ranges, unit)
            ok = (m["in_exec"] == n and m["zero_len"] == 0 and m["misaligned"] == 0
                  and m["overlap"] == 0 and m["past_end"] == 0)
            print(f"  arm {arm}/{unit_name:<12} "
                  f"in_exec {m['in_exec']:>6}/{n}  zero {m['zero_len']:>5}  "
                  f"misaligned {m['misaligned']:>5}  overlap {m['overlap']:>6}  "
                  f"past_end {m['past_end']:>5}  covered {m['covered']:>10,} "
                  f"({100.0 * m['covered'] / m['exec_bytes']:.1f}% of exec)"
                  f"{'   <-- clean' if ok else ''}")
            if ok and best is None:
                best = (arm, unit, ent, m)

    print()
    if best is None:
        print("NO ARM IS CLEAN. The entry layout assumed here is wrong; "
              "nothing downstream should use these numbers.")
        return 2

    arm, unit, ent, m = best
    print(f"adopted: {arm}, length counts "
          f"{'instructions' if unit == 4 else 'bytes'}")
    print(f"  {m['n']:,} function(s), covering {m['covered']:,} of "
          f"{m['exec_bytes']:,} executable byte(s) "
          f"({100.0 * m['covered'] / m['exec_bytes']:.1f}%)")
    per = {}
    for b, _p, l, _f, _e in ent:
        for lo, hi, nm in exec_ranges:
            if lo <= b < hi:
                per[nm] = per.get(nm, 0) + 1
                break
    print("  by section: " + ", ".join(f"{k} {v:,}" for k, v in sorted(per.items())))

    sizes = [l * unit for _b, _p, l, _f, _e in ent]
    sizes.sort()
    print(f"  size    min {sizes[0]}  median {sizes[len(sizes)//2]}  "
          f"p90 {sizes[int(len(sizes)*0.9)]}  max {sizes[-1]}")
    exc = sum(1 for e in ent if e[4])
    f32 = sum(1 for e in ent if e[3])
    print(f"  with exception data: {exc:,} of {m['n']:,}")
    print(f"  32-bit epilog flag:  {f32:,} of {m['n']:,}")

    starts = sorted(e[0] for e in ent)
    gaps = Counter()
    total_gap = 0
    for i in range(len(ent) - 1):
        b, _p, l, _f, _e = ent[i]
        g = ent[i + 1][0] - (b + l * unit)
        if g:
            gaps[g] += 1
            total_gap += g
    print(f"  inter-function gap:  {total_gap:,} byte(s) in {sum(gaps.values()):,} gap(s)")
    print(f"    most common: {', '.join(f'{g}B x{c}' for g, c in gaps.most_common(6))}")

    outp = Path("build/functions.txt")
    outp.parent.mkdir(parents=True, exist_ok=True)
    with outp.open("w") as f:
        f.write(f"# {m['n']} functions from .pdata, arm={arm}, unit={unit}\n")
        f.write("# address  size  prolog  exc\n")
        for b, p, l, _f32, e in ent:
            f.write(f"{b:08X} {l*unit:8d} {p*unit:6d} {e:3d}\n")
    print(f"\nwrote {outp}  ({m['n']:,} rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
