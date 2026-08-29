"""Name every kernel/XAM import the title uses.

The XEX header carries an import table: for each library, a list of guest
addresses. Each import occupies two records -- a data slot in .rdata and a
16-byte thunk in .text -- and the dword at each holds

    (type << 24) | ordinal

with type 0 a variable, 1 and 2 the two loader-patched words of the thunk.
That gives ordinals but not names.

The names come from the XDK's own import libraries, which carry short-import
records (`Sig1 == 0, Sig2 == 0xFFFF`) pairing an ordinal with a symbol.
`xboxkrnl.lib` identifies itself as `xboxkrnl.exe@8276.0`, the same XDK build
the title was compiled with, so the two sides are from one source rather than
a table written from memory.

Every join is reported with its denominator: an ordinal with no name is
counted, not dropped.
"""

import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image
from libmatch import archive_members

XEX = Path("game/DEFAULT.XEX")
LIBDIR = Path("SDKFiles/xdk/XDK/lib/xbox")
IMPORT_LIBRARIES_OFFSET = 0x28E4       # optional header 0x0103, from tools/xex.py


def read_xex_imports():
    d = XEX.read_bytes()
    o = IMPORT_LIBRARIES_OFFSET
    size, strtab_size, count = struct.unpack_from(">III", d, o)
    p = o + 12
    strs, cur = [], b""
    for b in d[p : p + strtab_size]:
        if b == 0:
            if cur:
                strs.append(cur.decode("latin1"))
            cur = b""
        else:
            cur += bytes([b])
    p += strtab_size
    libs = []
    for _ in range(count):
        lib_size = struct.unpack_from(">I", d, p)[0]
        name_idx, n = struct.unpack_from(">HH", d, p + 0x24)
        addrs = struct.unpack_from(">%dI" % n, d, p + 0x28)
        libs.append((strs[name_idx] if name_idx < len(strs) else "?", list(addrs)))
        p += lib_size
    return libs


def read_ordinal_names():
    """(dll_lower, ordinal) -> symbol, from every XDK import library."""
    out = {}
    per_lib = Counter()
    for lib in sorted(LIBDIR.glob("*.lib")):
        try:
            members = list(archive_members(lib.read_bytes()))
        except (ValueError, MemoryError):
            continue
        for _name, blob in members:
            if len(blob) < 22:
                continue
            if struct.unpack_from("<HH", blob, 0) != (0, 0xFFFF):
                continue
            ordhint = struct.unpack_from("<H", blob, 16)[0]
            parts = blob[20:].split(b"\0")
            if len(parts) < 2:
                continue
            sym = parts[0].decode("latin1")
            dll = parts[1].decode("latin1")
            key = dll.split("@")[0].lower()
            if (key, ordhint) not in out:
                out[(key, ordhint)] = sym
                per_lib[key] += 1
    return out, per_lib


def main():
    img = Image()
    libs = read_xex_imports()
    names, per_lib = read_ordinal_names()

    print("ordinal->name records read from the XDK import libraries:")
    for k, v in per_lib.most_common():
        print("   %-24s %5d" % (k, v))
    print()

    st = Counter()
    rows = []
    for dll, addrs in libs:
        key = dll.lower()
        seen = {}
        for a in addrs:
            b = img.read(a, 4)
            if b is None:
                st["unbacked"] += 1
                continue
            val = struct.unpack(">I", b)[0]
            typ = val >> 24
            ordinal = val & 0xFFFF
            st["records"] += 1
            st["type_%d" % typ] += 1
            if typ == 1:
                # First word of the thunk: this address IS the stub.
                nm = names.get((key, ordinal))
                if nm is None:
                    st["no_name"] += 1
                    nm = "%s_ord_%d" % (key.split(".")[0], ordinal)
                else:
                    st["named"] += 1
                seen[a] = (ordinal, nm)
        for a, (ordinal, nm) in sorted(seen.items()):
            rows.append((a, dll, ordinal, nm))

    print("import records walked: %d" % st["records"])
    for t in (0, 1, 2):
        if st["type_%d" % t]:
            print("   type %d %s : %d" % (t,
                  {0: "(variable slot)", 1: "(thunk)     ", 2: "(thunk word2)"}[t],
                  st["type_%d" % t]))
    print()
    print("thunks: %d named, %d with no ordinal match (%d total)"
          % (st["named"], st["no_name"], st["named"] + st["no_name"]))

    out = Path("build/imports.txt")
    with out.open("w") as f:
        f.write("# thunk_address library ordinal symbol\n")
        for a, dll, ordinal, nm in sorted(rows):
            f.write("%08X %s %d %s\n" % (a, dll, ordinal, nm))
    print("wrote %s (%d thunk(s))" % (out, len(rows)))

    print()
    print("sample:")
    for a, dll, ordinal, nm in sorted(rows)[:16]:
        print("   %08X  %-13s ord %4d  %s" % (a, dll, ordinal, nm))
    return 0


if __name__ == "__main__":
    sys.exit(main())
