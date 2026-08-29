"""Repair the XDK tools that ship without a VC90 activation context.

Fifteen modules in XDK/bin/win32 bind to the side-by-side VC90 CRT with no
manifest declaring it, so they die with R6034 the first time they are run.
link.exe was repaired once by hand with mt.exe; this generalises that, and
prefers a gentler mechanism.

An EXE with no embedded manifest is covered by an external
"<name>.exe.manifest" file beside it. Microsoft shipped cl.exe that way in
this very folder, which is why cl.exe works here and lib.exe does not. So the
repair is to write the same file for the others:

  * it modifies no binary, so nothing is at risk of corruption
  * it is undone by deleting one file
  * it needs no Windows SDK, so it works on a bare extraction

DLLs are left alone deliberately. A DLL runs inside whatever activation
context its host process established, so c1.dll/c2.dll are fine inside cl.exe
and pgodb90.dll is fine inside the repaired link.exe. Their missing manifests
are not the fault to fix.

    python tools/fix_manifests.py           # report what it would do
    python tools/fix_manifests.py --write   # do it
    python tools/fix_manifests.py <dir>...  # other tool directories

With no directory it repairs both toolchains the XDK ships: the default one in
bin/win32 and the TechPreview Mar09Compiler, whose cl.exe fails SILENTLY --
it exits non-zero and prints nothing at all, which reads like a broken command
line rather than a missing activation context.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pemanifest

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DIRS = [
    ROOT / "SDKFiles/xdk/XDK/bin/win32",
    ROOT / "SDKFiles/xdk/XDK/TechPreview/Mar09Compiler/bin/win32",
]

# Byte-identical in intent to the link.fix.manifest that repaired link.exe.
MANIFEST = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
 <dependency>
  <dependentAssembly>
   <assemblyIdentity type="win32" name="Microsoft.VC90.CRT" version="1.9.7.21022" processorArchitecture="x86" publicKeyToken="1fc8b3b9a1e18e3b"/>
  </dependentAssembly>
 </dependency>
</assembly>
"""


def broken_in(d):
    out = []
    for p in sorted(d.iterdir()):
        if p.suffix.lower() != ".exe":
            continue
        try:
            r = pemanifest.check(p)
        except Exception:
            continue
        if r and r[0] == "BROKEN":
            out.append(p)
    return out


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    write = "--write" in argv[1:]
    dirs = [Path(a) for a in args] if args else DEFAULT_DIRS

    total = 0
    for d in dirs:
        if not d.is_dir():
            print("%s does not exist, skipped" % d)
            continue
        broken = broken_in(d)
        print("%s" % d)
        if not broken:
            print("   no EXE here is missing a VC90 activation context.")
            continue
        total += len(broken)
        for p in broken:
            if not write:
                print("   would repair %s" % p.name)
                continue
            side = p.with_name(p.name + ".manifest")
            if side.exists():
                print("   %-16s already has %s, left alone" % (p.name, side.name))
                continue
            side.write_text(MANIFEST)
            print("   %-16s wrote %s" % (p.name, side.name))
        print("")

    if not write and total:
        print("re-run with --write to place an external manifest beside each.")
    elif write:
        print("Verify with a harmless invocation, e.g.:  dumpbin.exe /?")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
