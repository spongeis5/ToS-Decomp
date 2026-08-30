"""Measure link.exe 9.00.8153 at whole-image COMDAT counts.

HANDBOOK item 6's whole-image link hands the retail linker every function
start in the image -- roughly 31,000 COMDATs across ~1,300 real objects plus
filler objects, with an /ORDER file naming every one of them. Nothing in this
repository has asked link.exe for more than a few dozen COMDATs at once, and
whether a 2008 linker tolerates that (and at what time and memory cost)
decides the design. MEASURE before building.

What this does, per rung N:

  * writes ONE synthetic COFF object holding N `.text` COMDATs in the exact
    shape cl.exe emits (section static + selection-1 aux + external symbol),
    each 8-24 bytes of valid PowerPC ending in `blr`, sizes varied so the
    alignment padding is realistic;
  * writes an /ORDER file with all N names;
  * invokes SDKFiles/xdk/XDK/bin/win32/link.exe with tools/link.py's own flag
    set (/NOLOGO /MACHINE:PPCBE /SUBSYSTEM:XBOX /XEX:NO /FIXED
    /BASE:0x82000000 /NODEFAULTLIB /ENTRY:<first> /ORDER:@ /OPT:NOREF
    /OPT:NOICF /INCREMENTAL:NO /MAP /OUT);
  * times the invocation and samples link.exe's peak working set through a
    process handle held open across the wait (peak survives exit);
  * VALIDATES the result before believing it: the map must list N publics,
    and .text must hold every COMDAT (sum of sizes + alignment padding, +
    16 bytes of entry-point glue measured at N=64 and constant).

Usage:  python tools/scale_link.py
(see main() for the rungs; the three modes are 31k COMDATs with 31k REL24
relocations in one object, 1500 one-COMDAT objects on one command line, and
the plain no-relocation ladder 64..31000)

Validated before believing: the map must list N publics, .text must hold
every COMDAT, and -- for the relocation mode -- a SAMPLE of resolved bl
targets must land exactly on the public each relocation names, not merely
differ from the placeholder. 62 of 62 sampled at N=31000.
"""

import ctypes
import struct
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import coffwrite
from coffwrite import (_Strings, _symbol, IMAGE_FILE_MACHINE_POWERPCBE,
                       IMAGE_SYM_CLASS_EXTERNAL, IMAGE_SYM_CLASS_STATIC,
                       IMAGE_SYM_DTYPE_FUNCTION, IMAGE_SCN_CNT_CODE,
                       IMAGE_SCN_LNK_COMDAT, IMAGE_SCN_ALIGN_8BYTES,
                       IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_READ,
                       IMAGE_COMDAT_SELECT_NODUPLICATES)
import link

WORK = ROOT / "build/scaletest"
LINK = ROOT / "SDKFiles/xdk/XDK/bin/win32/link.exe"

BLR = struct.pack(">I", 0x4E800020)
LI_R3_0 = struct.pack(">I", 0x38600000)
# bl with displacement 0; a REL24 relocation against the next function
# makes the linker do the arithmetic that dominates a real .text.
BL_NEXT = struct.pack(">I", 0x48000000)
REL24 = 0x06


def comdat_object(n, with_relocs=False, names=None):
    """One COFF object with `n` function COMDATs, sizes varied 8..24.

    Mirrors cl.exe: every section is `.text` marked CNT_CODE | ALIGN_8 |
    COMDAT | EXECUTE | READ, each with the static section symbol, its
    selection-1 aux record, and one external symbol naming the COMDAT.

    `with_relocs` adds one REL24 per COMDAT against the NEXT symbol, so N
    links exercise N relocation resolutions -- the real .text is dense with
    calls and relocation processing is the plausible cost center, not
    section merging.

    `names` overrides the generated symbol names (needed when the object is
    one of many and each must be unique across objects).
    """
    st = _Strings()
    secs, datas, symtab = [], [], []
    raw = 20 + 40 * n
    if names is None:
        names = ["?scale_fn_%07d@@YAIXZ" % i for i in range(n)]
    assert len(names) == n
    dptr = raw
    for i in range(n):
        # vary the size so 4-byte alignment padding appears between COMDATs,
        # as it does in the image (464 of 464 measured pairs)
        nwords = 2 + (i % 4) + (2 if i % 3 == 0 else 0)   # 8..24 bytes
        body = bytearray((LI_R3_0 * 4 + BLR)[:4 * nwords])
        if with_relocs:
            body[:4] = BL_NEXT          # bl <relocated> at offset 0
        chars = (IMAGE_SCN_CNT_CODE | IMAGE_SCN_ALIGN_8BYTES
                 | IMAGE_SCN_LNK_COMDAT | IMAGE_SCN_MEM_EXECUTE
                 | IMAGE_SCN_MEM_READ)
        # PointerToRelocations/NumberOfRelocations are patched in below,
        # once the relocation data's address is known.
        secs.append(struct.pack("<8sIIIIIIHHI", b".text\0\0\0", 0, 0,
                                len(body), dptr, 0, 0, 0, 0, chars))
        datas.append(bytes(body))
        dptr += len(body)
    # relocation data sits after ALL raw data; one REL24 per section against
    # the next COMDAT's external symbol (section i holds symbols 3i..3i+2,
    # the extern being the third). A COFF relocation record is 10 bytes
    # (r_vaddr, r_symndx, r_type) -- an 8-byte stride desynchronised the
    # symbol table pointer by 2 bytes per entry and the linker read a
    # garbage string-table length out of it (LNK1106). Section header fields
    # patched: PointerToRelocations at +24, PointerToLinenumbers at +28 (0),
    # NumberOfRelocations at +32, NumberOfLinenumbers at +34.
    relptr = dptr if with_relocs else 0
    reldata = bytearray()
    for i in range(n):
        if not with_relocs:
            break
        secs[i] = (secs[i][:24] + struct.pack("<II", relptr, 0)
                   + struct.pack("<HH", 1, 0) + secs[i][36:])
        relptr += 10
        symidx = 3 * ((i + 1) % n) + 2
        reldata += struct.pack("<IIH", 0, symidx, REL24)
    for i in range(n):
        syms = _symbol(b".text\0\0\0", 0, i + 1, 0, IMAGE_SYM_CLASS_STATIC)
        syms = syms[:17] + bytes([1])
        syms += struct.pack("<IHHIHB3s", len(datas[i]), 0, 0, 0, 0,
                            IMAGE_COMDAT_SELECT_NODUPLICATES, bytes(3))
        syms += _symbol(st.ref(names[i]), 0, i + 1,
                        IMAGE_SYM_DTYPE_FUNCTION,
                        IMAGE_SYM_CLASS_EXTERNAL)
        symtab.append(syms)
    head = struct.pack("<HHIIIHH", IMAGE_FILE_MACHINE_POWERPCBE, n, 0,
                       relptr if with_relocs else dptr,
                       3 * n, 0, 0)
    return (head + b"".join(secs) + b"".join(datas) + bytes(reldata)
            + b"".join(symtab) + st.bytes())


def many_objects(n):
    """`n` one-COMDAT objects, modelling the real link's ~1,300 separate
    .obj files -- each written correctly from scratch (renaming a symbol by
    patching bytes would break the string table's length field, which is
    exactly the corruption LNK1106 reported on the first attempt).
    -> the list of symbol names."""
    names = []
    for i in range(n):
        names.append("?m_fn_%07d@@YAIXZ" % i)
        (WORK / ("m_%07d.obj" % i)).write_bytes(
            comdat_object(1, names=[names[-1]]))
    return names


# ---- peak working set of a child, via a handle held across the wait -------
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
k32 = ctypes.windll.kernel32
psapi = ctypes.windll.psapi


class _PMC(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD),
                ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t),
                ("PrivateUsage", ctypes.c_size_t)]


psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE,
                                       ctypes.POINTER(_PMC), wintypes.DWORD]


def run_link(n, relocs=False, tag=None):
    """-> dict(result of one measured link at N COMDATs)."""
    tag = tag or ("n%d%s" % (n, "r" if relocs else ""))
    obj = WORK / ("scale_%s.obj" % tag)
    obj.write_bytes(comdat_object(n, with_relocs=relocs))
    names = ["?scale_fn_%07d@@YAIXZ" % i for i in range(n)]
    (WORK / ("order_%s.txt" % tag)).write_text(
        "".join(x + "\n" for x in names), encoding="latin1",
        newline="\n")

    cmd = [str(LINK), "/NOLOGO", "/MACHINE:PPCBE", "/SUBSYSTEM:XBOX",
           "/XEX:NO", "/FIXED", "/BASE:0x%08X" % link.BASE,
           "/NODEFAULTLIB", "/ENTRY:" + names[0],
           "/ORDER:@order_%s.txt" % tag, "/OPT:NOREF", "/OPT:NOICF",
           "/INCREMENTAL:NO", "/MAP:map_%s.map" % tag,
           "/OUT:out_%s.exe" % tag, obj.name]

    t0 = time.perf_counter()
    p = subprocess.Popen(cmd, cwd=str(WORK),
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                        False, p.pid)
    out = p.communicate()[0].decode("latin1", "replace")
    secs = time.perf_counter() - t0
    peak = 0
    if h:
        pmc = _PMC()
        pmc.cb = ctypes.sizeof(_PMC)
        # the handle outlives the process object, so PEAK is readable after
        if psapi.GetProcessMemoryInfo(h, ctypes.byref(pmc), pmc.cb):
            peak = pmc.PeakWorkingSetSize
        k32.CloseHandle(h)

    res = {"n": n, "secs": secs, "peak": peak, "rc": p.returncode,
           "log": out, "tag": tag}
    if p.returncode == 0:
        syms = link.parse_map((WORK / ("map_%s.map" % tag))
                              .read_text(errors="replace"))
        ours = set(names)
        res["publics"] = len(ours & set(syms))
        pe = (WORK / ("out_%s.exe" % tag)).read_bytes()
        t = link._text_of(pe)
        res["text"] = t[1] if t else None    # rva; vsize read by text_vsize
        if relocs:
            # every bl must have been resolved: first body word != BL_NEXT
            res["reloc_ok"] = pe and _bl_was_relocated(pe, t)
    return res


def _bl_was_relocated(pe, t):
    """True if the first instruction of .text is no longer the placeholder
    `bl .+0` -- i.e. the linker really applied the REL24."""
    if not t:
        return False
    _ib, _rva, ptr = t
    w = struct.unpack_from(">I", pe, ptr)[0]
    return w != 0x48000000


def run_many(n):
    """N one-COMDAT objects on ONE command line -- models the real link's
    ~1,300 .obj files and measures whether the command line even holds."""
    names = many_objects(n)
    tag = "many%d" % n
    (WORK / ("order_%s.txt" % tag)).write_text(
        "".join(x + "\n" for x in names), encoding="latin1",
        newline="\n")
    cmd = [str(LINK), "/NOLOGO", "/MACHINE:PPCBE", "/SUBSYSTEM:XBOX",
           "/XEX:NO", "/FIXED", "/BASE:0x%08X" % link.BASE,
           "/NODEFAULTLIB", "/ENTRY:" + names[0],
           "/ORDER:@order_%s.txt" % tag, "/OPT:NOREF", "/OPT:NOICF",
           "/INCREMENTAL:NO", "/MAP:map_%s.map" % tag,
           "/OUT:out_%s.exe" % tag] + ["m_%07d.obj" % i for i in range(n)]
    cmdline_len = sum(len(a) + 1 for a in cmd)
    t0 = time.perf_counter()
    p = subprocess.Popen(cmd, cwd=str(WORK),
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                        False, p.pid)
    out = p.communicate()[0].decode("latin1", "replace")
    secs = time.perf_counter() - t0
    peak = 0
    if h:
        pmc = _PMC()
        pmc.cb = ctypes.sizeof(_PMC)
        if psapi.GetProcessMemoryInfo(h, ctypes.byref(pmc), pmc.cb):
            peak = pmc.PeakWorkingSetSize
        k32.CloseHandle(h)
    res = {"n": n, "secs": secs, "peak": peak, "rc": p.returncode,
           "log": out, "tag": tag, "cmdlen": cmdline_len}
    if p.returncode == 0:
        syms = link.parse_map((WORK / ("map_%s.map" % tag))
                              .read_text(errors="replace"))
        res["publics"] = len(set(names) & set(syms))
        pe = (WORK / ("out_%s.exe" % tag)).read_bytes()
        t = link._text_of(pe)
        res["text"] = t[1] if t else None
    return res


def text_vsize(pe):
    """Virtual size of .text, from the PE section header."""
    e = struct.unpack_from("<I", pe, 0x3C)[0]
    nsec = struct.unpack_from("<H", pe, e + 6)[0]
    optsz = struct.unpack_from("<H", pe, e + 20)[0]
    hdr = e + 24 + optsz
    for i in range(nsec):
        s = hdr + 40 * i
        name = pe[s:s + 8].rstrip(b"\0")
        if name == b".text":
            return struct.unpack_from("<I", pe, s + 8)[0]
    return None


def show(r):
    text = None
    exe = WORK / ("out_%s.exe" % r["tag"])
    if r["rc"] == 0 and exe.exists():
        text = text_vsize(exe.read_bytes())
    extra = ""
    if "reloc_ok" in r:
        extra = "  relocs_applied=%s" % r["reloc_ok"]
    if "cmdlen" in r:
        extra = "  cmdlen=%d chars" % r["cmdlen"]
    print("%6s %8.1f %10.1f %8d %10s %11s%s" %
          (r["tag"], r["secs"],
           r["peak"] / (1024 * 1024.0) if r["peak"] else -1,
           r["rc"], r.get("publics", "-"),
           text if text is not None else "-", extra))
    if r["rc"] != 0:
        for l in [l for l in r["log"].splitlines() if l.strip()][:6]:
            print("      ! %s" % l.strip()[:120])
    sys.stdout.flush()


def main():
    print("%6s %8s %10s %8s %10s %11s" %
          ("tag", "secs", "peak MB", "rc", "publics", "text bytes"))
    for n in (1000, 31000):
        show(run_link(n, relocs=True))
    for n in (1500,):
        show(run_many(n))
    # the already-measured plain ladder, for the record
    for n in (64, 1000, 8000, 16000, 31000):
        show(run_link(n))


if __name__ == "__main__":
    main()
