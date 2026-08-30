"""Build the mnemonic -> intrinsic map, which is what MATCHING needs.

Reading VMX128 is solved.  Writing it is not: to match a function containing
`vmsum3fp128` you must write C++ that compiles back to that exact
instruction, and nothing so far says which intrinsic does that.

This compiles every vector intrinsic the XDK headers declare, one per
statement, reads the `/FAsc` listing, and records which mnemonic each one
produced.  The result is the bridge from disassembly to source.

It also answers the question that decides whether VMX128 is really finished:
**is every form present in the retail image reachable from some intrinsic?**
A form with no intrinsic cannot be written in C++ at all, and would be a hard
blocker for any function containing it.
"""

import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import struct

XDK = Path("SDKFiles/xdk/XDK")
CL = XDK / "bin/win32/cl.exe"
INCLUDE = XDK / "include/xbox"
WORK = Path("build/vmxintrin")
HEADERS = ["ppcintrinsics.h", "vectorintrinsics.h"]

LINE = re.compile(r"^\s*[0-9a-f]+\s+([0-9a-f]{8})\s+(\S+)")
DECL = re.compile(r"\b(__(?:v|lv|stv)[a-z0-9_]+)\s*\(")


def declared_intrinsics():
    out = set()
    for h in HEADERS:
        p = INCLUDE / h
        if not p.exists():
            continue
        for m in DECL.finditer(p.read_text(errors="replace")):
            n = m.group(1)
            if not n.endswith("_volatile"):
                out.add(n)
    return sorted(out)


def try_compile(body):
    WORK.mkdir(parents=True, exist_ok=True)
    src = WORK / "t.cpp"
    src.write_text(body)
    for f in ("t.asm", "t.obj"):
        try:
            (WORK / f).unlink()
        except OSError:
            pass
    env = {"PATH": str((XDK / "bin/win32").resolve()),
           "INCLUDE": str(INCLUDE.resolve()),
           "SystemRoot": "C:\\Windows", "TEMP": str(WORK.resolve())}
    r = subprocess.run([str(CL.resolve()), "/c", "/nologo", "/O2", "/FAsc",
                        "/Fat.asm", "t.cpp"],
                       capture_output=True, text=True,
                       cwd=str(WORK.resolve()), env=env)
    if r.returncode != 0 or not (WORK / "t.asm").exists():
        return None
    return (WORK / "t.asm").read_text(errors="replace")


HEAD = ("#include <ppcintrinsics.h>\n#include <vectorintrinsics.h>\n\n")


N_LIVE = 40          # live vectors, enough to force the allocator past vr31

# The filler itself compiles to lvx128/stvx128, so excluding those as "noise"
# makes it impossible to ever credit __lvx with producing lvx128 -- which is
# what the second version of this tool did, reporting the two most common
# instructions in the image as unreachable from C++.
#
# Instead: measure a BASELINE listing with no intrinsic call, and attribute to
# the intrinsic any mnemonic whose COUNT rises above it. That works for loads
# and stores as well as arithmetic.


def pressure_body(call):
    """A function with N_LIVE vectors live across `call`.

    Register pressure is the whole point: with three live vectors the compiler
    emits the PLAIN VMX form (vor, vr0..vr31) and never the 128 form. A probe
    without pressure reports "no VMX128 intrinsic exists" for intrinsics that
    demonstrably have one -- which is what the first version of this tool did.
    """
    L = ["__vector4 f(__vector4 *o, const __vector4 *a, const __vector4 *b, int i)",
         "{"]
    for i in range(N_LIVE):
        L.append("    __vector4 s%02d = a[%d];" % (i, i))
    L.append("    __vector4 r = " + call + ";")
    for i in range(N_LIVE):
        L.append("    o[%d] = s%02d;" % (i, i))
    L.append("    return r;")
    L.append("}")
    return "\n".join(L) + "\n"


def counts_of(listing):
    c = Counter()
    for line in listing.splitlines():
        m = LINE.match(line)
        if m and m.group(2).endswith("128"):
            c[m.group(2)] += 1
    return c


BASELINE = {}


def make_baseline():
    listing = try_compile(HEAD + pressure_body("s00"))
    if listing is None:
        raise SystemExit("baseline probe did not compile")
    BASELINE.update(counts_of(listing))
    return BASELINE


def probe_one(name):
    """VMX128 mnemonics whose count rises above the baseline for this call."""
    calls = [
        "%s(s00, s01)" % name,
        "%s(s00)" % name,
        "%s(s00, s01, s02)" % name,
        "%s(s00, 3)" % name,
        "%s(s00, s01, 3)" % name,
        "%s(s00, 1, 2, 3)" % name,
        "%s((const void*)b, 0)" % name,
        "%s((const void*)b, i)" % name,
    ]
    best = (set(), None, 0)
    for c in calls:
        listing = try_compile(HEAD + pressure_body(c))
        if listing is None:
            continue
        got = counts_of(listing)
        gained = {m for m in got if got[m] > BASELINE.get(m, 0)}
        if gained and len(gained) > best[2] * -1:
            # Prefer the shape producing the FEWEST new mnemonics: that is the
            # one whose effect is the intrinsic alone rather than a sequence.
            if best[1] is None or len(gained) < len(best[0]):
                best = (gained, c, 0)
    return best[0], best[1]


def probe_store(name):
    """Stores return void, so they need their own call shapes."""
    for c in ["%s(s00, (void*)o, 0)" % name, "%s(s00, (void*)o, i)" % name]:
        body = HEAD + pressure_body("s00").replace(
            "    __vector4 r = s00;", "    __vector4 r = s00; " + c + ";")
        listing = try_compile(body)
        if listing is None:
            continue
        got = counts_of(listing)
        gained = {m for m in got if got[m] > BASELINE.get(m, 0)}
        if gained:
            return gained, c
    return set(), None


def image_forms():
    """Every VMX128 mnemonic actually present in .text, with counts."""
    c = Counter()
    p = Path("build/text_dis.txt")
    if not p.exists():
        raise SystemExit(chr(10).join([
            "build/text_dis.txt is missing.",
            "  Returning an empty census here would report ZERO VMX128",
            "  forms in the image, which reads exactly like a fact about",
            "  the image rather than a missing input -- and the whole",
            "  point of this check is to find forms with no intrinsic.",
            "  Produce it with:  python tools/dumptext.py",
        ]))
    for line in p.read_text(errors="replace").splitlines():
        parts = line.split(None, 2)
        if len(parts) < 3:
            continue
        m = parts[2].split()[0] if parts[2].split() else ""
        if m.endswith("128"):
            c[m] += 1
    return c


def main():
    names = declared_intrinsics()
    print("vector intrinsics declared in the XDK headers: %d" % len(names))
    base = make_baseline()
    print("baseline (filler only) emits: %s"
          % ", ".join("%s x%d" % (k, v) for k, v in sorted(base.items())))

    mapping = defaultdict(set)
    shape_of = {}
    ok = fail = 0
    for n in names:
        v, shape = probe_store(n) if n.startswith("__stv") else probe_one(n)
        if v:
            ok += 1
            for m in v:
                mapping[m].add(n)
                shape_of.setdefault((m, n), shape)
        else:
            fail += 1
    print("  compiled to at least one VMX128 form : %d" % ok)
    print("  no shape compiled / emitted no VMX128 : %d" % fail)
    print("  distinct VMX128 mnemonics reachable   : %d" % len(mapping))

    forms = image_forms()
    print("\nVMX128 forms present in .text: %d" % len(forms))
    covered = [m for m in forms if m in mapping]
    missing = [m for m in forms if m not in mapping]
    cov_n = sum(forms[m] for m in covered)
    mis_n = sum(forms[m] for m in missing)
    tot = sum(forms.values())
    print("  reachable from an intrinsic : %d form(s), %d instruction(s) (%.1f%%)"
          % (len(covered), cov_n, 100.0 * cov_n / max(tot, 1)))
    print("  NOT reachable               : %d form(s), %d instruction(s) (%.1f%%)"
          % (len(missing), mis_n, 100.0 * mis_n / max(tot, 1)))

    if missing:
        print("\n  forms in the image with NO intrinsic found -- these cannot")
        print("  currently be written in C++ and would block any function using them:")
        for m in sorted(missing, key=lambda x: -forms[x]):
            print("     %-16s %6d site(s)" % (m, forms[m]))

    out = Path("build/vmx128_intrinsics.txt")
    with out.open("w") as f:
        f.write("# mnemonic sites intrinsic(s)\n")
        for m in sorted(mapping):
            f.write("%-16s %6d  %s\n"
                    % (m, forms.get(m, 0), " ".join(sorted(mapping[m]))))
    print("\nwrote %s" % out)

    print("\n  the map, for the forms that actually occur (by frequency):")
    for m in sorted(forms, key=lambda x: -forms[x])[:22]:
        ins = " ".join(sorted(mapping.get(m, []))) or "(none found)"
        print("     %-16s %6d  <- %s" % (m, forms[m], ins))
    return 0


if __name__ == "__main__":
    sys.exit(main())
