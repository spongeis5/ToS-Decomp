"""Find every reference to a guest address.

PowerPC has no absolute 32-bit load, so an address is built from a pair:

    lis  rD, hi          addis rD, r0, hi     opcode 15, rA == 0
    addi rD, rD, lo      opcode 14, SIGNED lo
  or
    ori  rA, rS, lo      opcode 24, UNSIGNED lo

The signed low half matters: for lo >= 0x8000 the compiler emits a HIGH half
one greater, so `hi:lo` read naively is off by 0x10000.  Both forms are
handled and the arm that produced each hit is reported, because a scan that
silently covers one form is a scan of a different population.

Data references are found too -- a plain big-endian dword equal to the
address, anywhere in the image.

    python tools/xref.py 82065B68
    python tools/xref.py 82065B68 --window 32

Every count states its denominator, and a bounded search says when its
bound was reached rather than reporting the bound as an answer.
"""

import struct
import sys
from pathlib import Path

IMAGE = Path("build/default.pe.exe")
DEFAULT_WINDOW = 16  # instructions to look ahead from a lis for its partner


def load(data):
    o = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, o + 6)[0]
    optsz = struct.unpack_from("<H", data, o + 20)[0]
    base = struct.unpack_from("<I", data, o + 24 + 28)[0]
    sh = o + 24 + optsz
    secs = []
    for i in range(nsec):
        b = sh + i * 40
        name = data[b : b + 8].rstrip(b"\0").decode("latin1")
        vsize, va, rawsz, rawptr = struct.unpack_from("<IIII", data, b + 8)
        chars = struct.unpack_from("<I", data, b + 36)[0]
        if rawptr == 0 or rawsz == 0:
            continue
        # RVA == offset in the unpacked buffer. PointerToRawData describes the
        # ORIGINAL file layout and is stale; reading through it walks the wrong
        # bytes for every section from .text onward.
        secs.append(dict(name=name, va=base + va, size=(vsize or rawsz),
                         ptr=va, chars=chars, exec=bool(chars & 0x20000000)))
    return base, secs


def functions():
    """The .pdata inventory, so a hit can be attributed to a function."""
    p = Path("build/functions.txt")
    if not p.exists():
        return []
    out = []
    for line in p.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        out.append((int(f[0], 16), int(f[1])))
    out.sort()
    return out


def owner(funcs, addr):
    lo, hi = 0, len(funcs) - 1
    best = None
    while lo <= hi:
        mid = (lo + hi) // 2
        if funcs[mid][0] <= addr:
            best = funcs[mid]
            lo = mid + 1
        else:
            hi = mid - 1
    if best and best[0] <= addr < best[0] + best[1]:
        return best[0]
    return None


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    target = int(argv[1], 16)
    window = int(argv[argv.index("--window") + 1]) if "--window" in argv else DEFAULT_WINDOW

    data = IMAGE.read_bytes()
    base, secs = load(data)
    funcs = functions()

    hits = []
    words_scanned = 0
    lis_seen = 0
    window_bound_reached = 0

    for s in secs:
        if not s["exec"]:
            continue
        n = s["size"] // 4
        words = struct.unpack_from(f">{n}I", data, s["ptr"])
        words_scanned += n
        for i, w in enumerate(words):
            if (w >> 26) != 15:
                continue
            rD = (w >> 21) & 0x1F
            rA = (w >> 16) & 0x1F
            if rA != 0:
                continue  # addis rD,rA,x is arithmetic, not a lis
            lis_seen += 1
            hi = w & 0xFFFF
            bound = True
            for j in range(i + 1, min(i + 1 + window, n)):
                w2 = words[j]
                op = w2 >> 26
                if op == 14:  # addi rD, rA, SIMM  -- dest 21-25, src 16-20
                    d2, a2 = (w2 >> 21) & 0x1F, (w2 >> 16) & 0x1F
                    if a2 == rD:
                        lo = w2 & 0xFFFF
                        if lo >= 0x8000:
                            lo -= 0x10000
                        val = ((hi << 16) + lo) & 0xFFFFFFFF
                        if val == target:
                            hits.append((s["va"] + i * 4, s["va"] + j * 4, "lis+addi", s["name"]))
                        if d2 == rD:
                            bound = False
                            break
                elif op == 24:  # ori rA, rS, UIMM -- dest 16-20, src 21-25
                    a2, s2 = (w2 >> 16) & 0x1F, (w2 >> 21) & 0x1F
                    if s2 == rD:
                        val = ((hi << 16) | (w2 & 0xFFFF)) & 0xFFFFFFFF
                        if val == target:
                            hits.append((s["va"] + i * 4, s["va"] + j * 4, "lis+ori", s["name"]))
                        if a2 == rD:
                            bound = False
                            break
                else:
                    # rD overwritten by something else ends the pairing
                    d2 = (w2 >> 21) & 0x1F
                    if op in (14, 15, 24, 25, 32, 36, 56) and d2 == rD:
                        bound = False
                        break
            else:
                if bound:
                    window_bound_reached += 1

    # Data references: a plain big-endian dword equal to the target.
    data_hits = []
    dwords_scanned = 0
    tb = struct.pack(">I", target)
    for s in secs:
        blob = data[s["ptr"] : s["ptr"] + s["size"]]
        dwords_scanned += len(blob) // 4
        start = 0
        while True:
            k = blob.find(tb, start)
            if k < 0:
                break
            if k % 4 == 0:
                data_hits.append((s["va"] + k, s["name"]))
            start = k + 1

    print(f"references to {target:08X}")
    print(f"  scanned {words_scanned:,} instruction word(s) in "
          f"{sum(1 for s in secs if s['exec'])} executable section(s), "
          f"{lis_seen:,} lis site(s)")
    print(f"  scanned {dwords_scanned:,} aligned dword(s) in {len(secs)} section(s)")
    print()

    print(f"  CODE: {len(hits)} hit(s)")
    for lis_at, lo_at, form, sec in hits:
        fn = owner(funcs, lis_at)
        where = f"sub_{fn:08X}+{lis_at - fn:#x}" if fn else "(no .pdata function)"
        print(f"    {lis_at:08X}  {form:<9} low half at {lo_at:08X}  {sec:<9} {where}")
    print()
    print(f"  DATA: {len(data_hits)} aligned dword(s) equal to the address")
    for at, sec in data_hits[:40]:
        print(f"    {at:08X}  in {sec}")
    if len(data_hits) > 40:
        print(f"    ... {len(data_hits) - 40} more")

    print()
    if window_bound_reached:
        print(f"  NOTE: {window_bound_reached:,} lis site(s) ran to the "
              f"{window}-instruction lookahead limit without their register "
              f"being reused or paired. Raise --window to see whether any of "
              f"those are hits; this count is about the BOUND, not the image.")
    else:
        print(f"  no lis site reached the {window}-instruction lookahead bound")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
