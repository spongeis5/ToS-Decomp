"""Mark the VMX128 decoder against MICROSOFT'S OWN ENCODER.

Two oracles were already used: binutils' opcode table, and Biallas's
`vmx128.txt`.  Both are third-party reconstructions -- Biallas's own preamble
says "I figured this out by looking at various disassemblies, so there might
be some errors", and Ghidra issue #2094 records someone later finding errors
in it.

The XDK ships the authoritative encoder.  `cl.exe /FAsc` emits a listing
carrying BOTH the encoded word and the register names it chose:

    0001c  17bef8b5   vmulfp128    vr61,vr62,vr63

That is ground truth: the compiler that built the retail image, stating what
it encoded and which of the 128 registers it meant.  The XDK's assembler
(ml.exe) is NOT usable for this -- it only defines vr0..vr31 and rejects the
`*128` arithmetic mnemonics outright, so it predates the extension.

The load on this check is the SPLIT REGISTER FIELDS.  A 7-bit register number
is scattered across the word (`VD = VDh:VD128`, `VA = A:a:VA128`,
`VB = VBh:VB128`), which is exactly what a hand transcription gets wrong and
exactly what a mnemonic-only comparison cannot catch.  So register pressure is
forced high enough to reach vr32..vr127, where those bits are non-zero.
"""

import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import ppcdis

XDK = Path("SDKFiles/xdk/XDK")
CL = XDK / "bin/win32/cl.exe"
INCLUDE = XDK / "include/xbox"
WORK = Path("build/vmxoracle")

# (intrinsic, arity) -- binary ones become u = f(a,b), unary u = f(a)
BINARY = ["__vmulfp", "__vaddfp", "__vsubfp", "__vor", "__vand", "__vxor",
          "__vandc", "__vnor", "__vmsum3fp", "__vmsum4fp", "__vmaxfp",
          "__vminfp", "__vmrghw", "__vmrglw", "__vslw", "__vsrw", "__vsraw",
          "__vrlw", "__vcmpeqfp", "__vcmpgtfp", "__vcmpgefp", "__vpkshss",
          "__vpkshus", "__vpkswss", "__vpkswus", "__vpkuhum", "__vpkuwum"]
UNARY = ["__vrefp", "__vrsqrtefp", "__vexptefp", "__vlogefp", "__vrfin",
         "__vrfiz", "__vrfip", "__vrfim", "__vupkhsb", "__vupklsb"]

LINE = re.compile(r"^\s*[0-9a-f]+\s+([0-9a-f]{8})\s+(\S+)\s+(\S+)")


def gen_source(n=32):
    L = ["#include <ppcintrinsics.h>", "#include <vectorintrinsics.h>", ""]
    L.append("void probe(__vector4 *o, const __vector4 *a, const __vector4 *b)")
    L.append("{")
    # Keep many vectors live at once so the allocator must use vr32..vr127.
    for i in range(n):
        L.append("    __vector4 s%02d = a[%d], t%02d = b[%d];" % (i, i, i, i))
    k = 0
    for i in range(n):
        f = BINARY[i % len(BINARY)]
        L.append("    __vector4 u%02d = %s(s%02d, t%02d);" % (i, f, i, i))
        k += 1
    for i, f in enumerate(UNARY):
        L.append("    __vector4 w%02d = %s(s%02d);" % (i, f, i % n))
    for i in range(n):
        L.append("    o[%d] = u%02d;" % (i, i))
    for i in range(len(UNARY)):
        L.append("    o[%d] = w%02d;" % (n + i, i))
    L.append("}")
    return "\n".join(L) + "\n"


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    src = WORK / "probe.cpp"
    src.write_text(gen_source())

    env = {"PATH": str((XDK / "bin/win32").resolve()),
           "INCLUDE": str(INCLUDE.resolve()),
           "SystemRoot": "C:\\Windows", "TEMP": str(WORK.resolve())}
    r = subprocess.run([str(CL.resolve()), "/c", "/nologo", "/O2", "/FAsc",
                        "/Faprobe.asm", "probe.cpp"],
                       capture_output=True, text=True,
                       cwd=str(WORK.resolve()), env=env)
    if r.returncode != 0:
        print("compile failed:\n" + r.stdout + r.stderr)
        return 2

    listing = (WORK / "probe.asm").read_text(errors="replace")

    rows = []
    for line in listing.splitlines():
        m = LINE.match(line)
        if not m:
            continue
        word, mnem, ops = m.group(1), m.group(2), m.group(3)
        rows.append((int(word, 16), mnem, ops))

    v128 = [r_ for r_ in rows if r_[1].endswith("128")]
    print("listing lines with an encoding : %d" % len(rows))
    print("  VMX128 instructions           : %d" % len(v128))

    regs = set()
    for _w, _m, ops in v128:
        for x in re.findall(r"vr(\d+)", ops):
            regs.add(int(x))
    high = sorted(x for x in regs if x > 31)
    print("  distinct vector registers used: %d  (%d above vr31: %s%s)"
          % (len(regs), len(high),
             ", ".join("vr%d" % x for x in high[:8]),
             " ..." if len(high) > 8 else ""))
    if not high:
        print("\nNO REGISTER ABOVE vr31 WAS EMITTED -- this check would only")
        print("exercise the low fields and cannot confirm the split. Raise the")
        print("register pressure in gen_source().")
        return 2

    st = Counter()
    bad = []
    for w, mnem, ops in v128:
        got = ppcdis.words([w], 0)[0][2]
        p = got.split(None, 1)
        gm = p[0]
        go = p[1].replace(" ", "") if len(p) > 1 else ""
        # binutils prints v63; the listing prints vr63
        want = ops.replace("vr", "v").replace(" ", "")
        st["checked"] += 1
        if gm == mnem and go == want:
            st["agree"] += 1
        elif gm == mnem:
            st["operands_differ"] += 1
            if len(bad) < 15:
                bad.append((w, mnem, want, got))
        else:
            st["mnemonic_differs"] += 1
            if len(bad) < 15:
                bad.append((w, mnem, want, got))

    print()
    print("checked                          : %d" % st["checked"])
    print("  AGREE, mnemonic AND registers  : %d" % st["agree"])
    print("  operands differ                : %d" % st["operands_differ"])
    print("  mnemonic differs               : %d" % st["mnemonic_differs"])
    if bad:
        print("\n  disagreements (Microsoft's encoder vs binutils):")
        for w, mnem, want, got in bad:
            print("    %08X  MSVC: %-13s %-24s binutils: %s" % (w, mnem, want, got))
    else:
        print("\n  No disagreement. The decoder reproduces Microsoft's own")
        print("  encoding, including the split 7-bit register numbers, on")
        print("  every VMX128 instruction the compiler emitted.")

    forms = Counter(m for _w, m, _o in v128)
    print("\n  forms exercised: %s" % ", ".join(
        "%s(%d)" % (k, v) for k, v in forms.most_common()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
