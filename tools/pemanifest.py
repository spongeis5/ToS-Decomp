"""Find which XDK tools will die with R6034, WITHOUT running them.

R6034 -- "an application has made an attempt to load the C runtime library
incorrectly" -- fires when a module binds to the side-by-side VC90 CRT
(msvcr90.dll / msvcp90.dll) but carries no embedded manifest declaring that
dependency, so there is no activation context to resolve it. link.exe shipped
in this XDK with an EMPTY manifest resource and had to be repaired; other
tools in the same folder have the same defect and will fail the same way the
first time they are invoked.

Running each tool to find out is the wrong instrument: it pops a modal dialog
that blocks until a human clicks OK. This reads the answer out of the file
instead.

    python tools/pemanifest.py <dir-or-file> [...]

A module is reported BROKEN when it imports the VC90 CRT and either has no
RT_MANIFEST resource or has one that does not name Microsoft.VC90.

Host PEs only -- this uses PointerToRawData, which is correct for ordinary
files on disk and is NOT correct for the unpacked game image (see
tools/peimage.py).
"""

import struct
import sys
from pathlib import Path

RT_MANIFEST = 24


class Bad(Exception):
    pass


def sections(data):
    if data[:2] != b"MZ":
        raise Bad("not a PE")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise Bad("no PE signature")
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    magic = struct.unpack_from("<H", data, pe + 24)[0]
    ddoff = pe + 24 + (96 if magic == 0x10B else 112)
    nrva = struct.unpack_from("<I", data, pe + 24 + (92 if magic == 0x10B else 108))[0]
    dirs = []
    for i in range(nrva):
        dirs.append(struct.unpack_from("<II", data, ddoff + i * 8))
    secs = []
    so = pe + 24 + optsz
    for i in range(nsec):
        name, vsz, va, rsz, ptr = struct.unpack_from("<8sIIII", data, so + i * 40)
        secs.append((va, vsz, rsz, ptr))
    return dirs, secs


def rva2off(secs, rva):
    for va, vsz, rsz, ptr in secs:
        if va <= rva < va + max(vsz, rsz):
            off = ptr + (rva - va)
            return off
    return None


def imports(data, dirs, secs):
    if len(dirs) < 2 or dirs[1][0] == 0:
        return []
    off = rva2off(secs, dirs[1][0])
    if off is None:
        return []
    names = []
    while True:
        try:
            fields = struct.unpack_from("<IIIII", data, off)
        except struct.error:
            break
        if not any(fields):
            break
        no = rva2off(secs, fields[3])
        if no is not None and no < len(data):
            end = data.index(b"\0", no)
            names.append(data[no:end].decode("latin1"))
        off += 20
    return names


def walk_res(data, secs, base, off, depth, want, out):
    try:
        nnamed, nid = struct.unpack_from("<HH", data, off + 12)
    except struct.error:
        return
    n = nnamed + nid
    for i in range(n):
        eo = off + 16 + i * 8
        try:
            ident, ptr = struct.unpack_from("<II", data, eo)
        except struct.error:
            return
        if depth == 0 and not (ident & 0x80000000) and ident != want:
            continue
        if ptr & 0x80000000:
            walk_res(data, secs, base, base + (ptr & 0x7FFFFFFF),
                     depth + 1, want, out)
        else:
            try:
                drva, size = struct.unpack_from("<II", data, base + ptr)
            except struct.error:
                continue
            do = rva2off(secs, drva)
            if do is not None:
                out.append(data[do:do + size])


def manifests(data, dirs, secs):
    if len(dirs) < 3 or dirs[2][0] == 0:
        return []
    base = rva2off(secs, dirs[2][0])
    if base is None:
        return []
    out = []
    walk_res(data, secs, base, base, 0, RT_MANIFEST, out)
    return out


def check(path):
    """-> (state, crt-imports, why) or None if the module cannot raise R6034.

    States:
      ok       carries its own VC90 activation context
      external an EXE with no embedded manifest but a <name>.manifest beside
               it, which the loader honours -- this is why cl.exe works
      inherit  a DLL with no manifest; it runs inside whatever activation
               context its HOST process established, so whether it fails
               depends on the host, not on this file
      BROKEN   an EXE with neither embedded nor external manifest
    """
    data = path.read_bytes()
    dirs, secs = sections(data)
    imps = [n.lower() for n in imports(data, dirs, secs)]
    crt = [n for n in imps if n in ("msvcr90.dll", "msvcp90.dll", "msvcm90.dll")]
    if not crt:
        return None
    mans = manifests(data, dirs, secs)
    joined = b"".join(mans)
    if joined.strip() and (b"VC90" in joined or b"vc90" in joined):
        return ("ok", crt, "embedded manifest declares VC90")
    why = ("no RT_MANIFEST resource" if not mans
           else "RT_MANIFEST present but EMPTY (%d bytes)" % len(joined)
           if not joined.strip() else "manifest does not name Microsoft.VC90")

    is_dll = path.suffix.lower() == ".dll"
    ext = path.with_name(path.name + ".manifest")
    if not is_dll and ext.exists():
        return ("external", crt, "%s; but %s exists" % (why, ext.name))
    if is_dll:
        return ("inherit", crt, "%s; DLL -- depends on the host process" % why)
    return ("BROKEN", crt, why)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    targets = []
    for a in argv[1:]:
        p = Path(a)
        if p.is_dir():
            targets += sorted(q for q in p.iterdir()
                              if q.suffix.lower() in (".exe", ".dll"))
        else:
            targets.append(p)

    buckets = {"ok": [], "external": [], "inherit": [], "BROKEN": []}
    skipped = unreadable = 0
    for p in targets:
        try:
            r = check(p)
        except (Bad, struct.error, ValueError, OSError):
            unreadable += 1
            continue
        if r is None:
            skipped += 1
            continue
        buckets[r[0]].append((p, r[1], r[2]))

    print("%d file(s) examined" % len(targets))
    print("  %d do not import the VC90 CRT (cannot raise R6034)" % skipped)
    if unreadable:
        print("  %d could not be parsed" % unreadable)
    print("  %d ok        -- embedded VC90 manifest" % len(buckets["ok"]))
    print("  %d external  -- EXE covered by a <name>.manifest beside it"
          % len(buckets["external"]))
    print("  %d inherit   -- DLL, fails only if its host lacks the context"
          % len(buckets["inherit"]))
    print("  %d BROKEN    -- EXE with no manifest at all" % len(buckets["BROKEN"]))
    for state in ("BROKEN", "inherit", "external", "ok"):
        if not buckets[state]:
            continue
        print("")
        for p, crt, why in buckets[state]:
            print("  %-9s %-24s %-22s %s"
                  % (state, p.name, ",".join(crt), why))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
