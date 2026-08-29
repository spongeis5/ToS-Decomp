"""Decode the MSVC "Rich" header of a PE.

The Rich header is a per-image manifest that link.exe writes into the DOS stub:
one (product-id, build, count) row for every tool that contributed objects. It
is not a version banner -- it is a census, and it distinguishes tools that a
version number cannot, because link-time codegen stamps its output with a
DIFFERENT product id from an ordinary compile.

That makes it the direct instrument for the LTCG question. We do not need the
community prodid table and we deliberately do not use it: the XDK is here, so
the meaning of each id can be MEASURED by building with a known flag set and
seeing which ids appear. See tools/rich_calibrate.py.

    python tools/rich.py <pe> [<pe> ...]

Layout, all little-endian, all within the DOS stub before e_lfanew:

    'DanS' ^ key | 3 padding dwords ^ key   <- decoded padding must be 0
    (compid ^ key, count ^ key) * n         <- compid = prodid<<16 | build
    'Rich' (PLAIN, not xored) | key

Self-checks, all of which refuse rather than return a plausible number:
  * 'Rich' must be found before e_lfanew
  * xoring back must reach 'DanS'
  * the span from DanS to Rich must be a whole number of dwords
  * the three dwords after DanS must decode to zero
  * the row count must be (span - 16) / 8 exactly
"""

import struct
import sys
from pathlib import Path


class NoRichHeader(Exception):
    pass


def decode(data):
    """Return (key, [(prodid, build, count), ...]) or raise NoRichHeader."""
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise NoRichHeader("not a PE (no MZ)")
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if not (0x40 <= e_lfanew <= len(data)):
        raise NoRichHeader("e_lfanew %#x out of range" % e_lfanew)

    stub = data[:e_lfanew]
    pos = stub.rfind(b"Rich")
    if pos < 0:
        raise NoRichHeader("no 'Rich' marker in the DOS stub")
    if pos + 8 > len(stub):
        raise NoRichHeader("'Rich' marker has no key after it")
    key = struct.unpack_from("<I", stub, pos + 4)[0]

    # Walk backwards in dword steps until the decoded word is 'DanS'.
    DANS = 0x536E6144
    start = None
    p = pos - 4
    while p >= 0:
        if struct.unpack_from("<I", stub, p)[0] ^ key == DANS:
            start = p
            break
        p -= 4
    if start is None:
        raise NoRichHeader("xor key %08X never reaches 'DanS'" % key)

    span = pos - start
    if span % 4:
        raise NoRichHeader("DanS..Rich span %d is not a whole dword count" % span)
    if span < 16:
        raise NoRichHeader("DanS..Rich span %d is shorter than the header" % span)

    words = list(struct.unpack_from("<%dI" % (span // 4), stub, start))
    words = [w ^ key for w in words]
    if words[1:4] != [0, 0, 0]:
        raise NoRichHeader("padding after DanS is %r, expected three zeros"
                           % words[1:4])
    body = words[4:]
    if len(body) % 2:
        raise NoRichHeader("%d words after the padding is not whole rows"
                           % len(body))

    rows = []
    for i in range(0, len(body), 2):
        compid, count = body[i], body[i + 1]
        rows.append(((compid >> 16) & 0xFFFF, compid & 0xFFFF, count))
    return key, rows


def report(path, rows, key):
    print("%s" % path)
    print("  xor key %08X, %d row(s)" % (key, len(rows)))
    print("  %-7s %-7s %s" % ("prodid", "build", "count"))
    for prodid, build, count in rows:
        print("  %-7d %-7d %d" % (prodid, build, count))


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    rc = 0
    for arg in argv[1:]:
        p = Path(arg)
        if not p.exists():
            print("%s: missing" % p)
            rc = 1
            continue
        try:
            key, rows = decode(p.read_bytes())
        except NoRichHeader as e:
            print("%s: NO RICH HEADER -- %s" % (p, e))
            rc = 1
            continue
        report(p, rows, key)
        print("")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
