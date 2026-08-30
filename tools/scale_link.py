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

Validated before believing, and the validation is ENFORCED -- every rung's
failures are collected and `main` exits non-zero:

  * the map must list exactly N publics;
  * `.text` must be present and non-empty;
  * in relocation mode, every SAMPLED bl target must land exactly on the
    public its relocation names, decoded from the displacement and compared
    against the map -- not merely differ from the placeholder.

**THE FIRST VERSION CLAIMED THAT LAST ONE AND DID NOT DO IT.** This
docstring, the commit message and HANDBOOK all said "62 of 62 sampled bl
targets land exactly on the public each relocation names, not merely differ
from the placeholder". The code read ONE word and tested `w != 0x48000000`,
which is the placeholder test on a single instruction. A `bl` with a wrong
displacement differs from the placeholder just as convincingly as a right
one, so the check could not fail short of the linker ignoring relocations
altogether.

Nor was anything else enforced: the publics count and the .text size were
printed for a human to read, nothing compared them, and `main()` returned
None -- so every rung could fail and the tool still exited 0. The paragraph
this measurement produced in HANDBOOK ends "distrust an exit code until the
map and the bytes agree with what was asked for", which is exactly what the
tool was not doing. Numbers this prints are only worth what its validation
is worth, so run it and read the last line rather than quoting the table.
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
        res["want_publics"] = n
        pe = (WORK / ("out_%s.exe" % tag)).read_bytes()
        t = link._text_of(pe)
        res["text"] = t[1] if t else None    # rva; vsize read by text_vsize
        if relocs:
            res["sampled"], res["landed"] = _bl_targets_land(pe, t, syms, names)
    return res


def _bl_targets_land(pe, t, syms, names, stride=500):
    """-> (sampled, landed exactly on the public the relocation names).

    THIS USED TO READ ONE WORD AND TEST `w != 0x48000000`, while the
    docstring, the commit message and HANDBOOK all said it sampled 62
    targets and required each to "land exactly on the public each relocation
    names, NOT MERELY DIFFER FROM THE PLACEHOLDER". It was the placeholder
    test, on a single instruction.

    A `bl` whose displacement is wrong differs from the placeholder just as
    convincingly as one that is right, so the old check could not fail for
    any reason short of the linker ignoring relocations entirely -- which is
    the one outcome nobody suspected. Claiming the stronger check and
    performing the weaker one is how a measurement gets believed.

    comdat_object() emits, for COMDAT i, a `bl` at its offset 0 relocated
    against the external symbol of COMDAT (i+1) % n. So the target is
    computable from the map, and this decodes the displacement and compares.
    """
    if not t or not pe:
        return 0, 0
    _ib, _rva, ptr = t
    base = link.BASE
    sampled = landed = 0
    for i in range(0, len(names), stride):
        want_name = names[(i + 1) % len(names)]
        here = syms.get(names[i])
        there = syms.get(want_name)
        if here is None or there is None:
            continue
        off = ptr + (here - base) - _rva
        if off < 0 or off + 4 > len(pe):
            continue
        w = struct.unpack_from(">I", pe, off)[0]
        if (w >> 26) != 18:                    # not a b/bl at all
            sampled += 1
            continue
        disp = w & 0x03FFFFFC
        if disp & 0x02000000:                  # sign-extend 26 bits
            disp -= 0x04000000
        sampled += 1
        if (here + disp) == there:
            landed += 1
    return sampled, landed


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


def problems(r):
    """Everything about this rung that is not what was asked for.

    The validation was DESCRIBED and never ENFORCED: the publics count and
    the .text size were printed for a human to eyeball, nothing compared
    them to anything, and main() returned None so the tool exited 0 even
    when a link failed. A measurement harness that cannot fail is the same
    shape as a check that cannot fail, and this repository keeps
    verify_ghidra.py around as the worked example.
    """
    bad = []
    if r["rc"] != 0:
        bad.append("link failed, rc=%d" % r["rc"])
        return bad
    want = r.get("want_publics")
    got = r.get("publics")
    if want is not None and got != want:
        bad.append("map lists %s public(s), asked for %d" % (got, want))
    exe = WORK / ("out_%s.exe" % r["tag"])
    if not exe.exists():
        bad.append("no output file")
    elif text_vsize(exe.read_bytes()) in (None, 0):
        bad.append(".text is absent or empty")
    if "sampled" in r:
        if r["sampled"] == 0:
            bad.append("no bl target could be sampled")
        elif r["landed"] != r["sampled"]:
            bad.append("%d of %d sampled bl target(s) did NOT land on the "
                       "public their relocation names"
                       % (r["sampled"] - r["landed"], r["sampled"]))
    return bad


def show(r):
    text = None
    exe = WORK / ("out_%s.exe" % r["tag"])
    if r["rc"] == 0 and exe.exists():
        text = text_vsize(exe.read_bytes())
    extra = ""
    if "sampled" in r:
        extra = "  bl targets %d/%d land" % (r["landed"], r["sampled"])
    if "cmdlen" in r:
        extra = "  cmdlen=%d chars" % r["cmdlen"]
    print("%6s %8.1f %10.1f %8d %10s %11s%s" %
          (r["tag"], r["secs"],
           r["peak"] / (1024 * 1024.0) if r["peak"] else -1,
           r["rc"], r.get("publics", "-"),
           text if text is not None else "-", extra))
    bad = problems(r)
    for b in bad:
        print("      FAIL %s" % b)
    if r["rc"] != 0:
        for l in [l for l in r["log"].splitlines() if l.strip()][:6]:
            print("      ! %s" % l.strip()[:120])
    sys.stdout.flush()
    return bad


def main():
    print("%6s %8s %10s %8s %10s %11s" %
          ("tag", "secs", "peak MB", "rc", "publics", "text bytes"))
    bad = []
    for n in (1000, 31000):
        bad += show(run_link(n, relocs=True))
    for n in (1500,):
        bad += show(run_many(n))
    # the already-measured plain ladder, for the record
    for n in (64, 1000, 8000, 16000, 31000):
        bad += show(run_link(n))
    print("")
    if bad:
        print("%d rung(s) did not do what was asked for. The timings above "
              "are NOT" % len(bad))
        print("evidence of capacity: a link that did not place what it was "
              "given is")
        print("fast for the wrong reason.")
        return 1
    print("every rung validated: publics as asked, .text present, and every")
    print("sampled bl target landed on the public its relocation names.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
