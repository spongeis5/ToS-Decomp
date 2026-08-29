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

    python tools/fix_manifests.py          # report what it would do
    python tools/fix_manifests.py --write  # do it
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pemanifest

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "SDKFiles/xdk/XDK/bin/win32"

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


def main(argv):
    write = "--write" in argv[1:]
    if not BIN.is_dir():
        print("%s does not exist" % BIN)
        return 1

    broken = []
    for p in sorted(BIN.iterdir()):
        if p.suffix.lower() != ".exe":
            continue
        try:
            r = pemanifest.check(p)
        except Exception:
            continue
        if r and r[0] == "BROKEN":
            broken.append(p)

    if not broken:
        print("no EXE in %s is missing a VC90 activation context." % BIN)
        return 0

    print("%d EXE(s) will raise R6034 as shipped:" % len(broken))
    for p in broken:
        print("   %s" % p.name)
    print("")

    if not write:
        print("re-run with --write to place an external manifest beside each.")
        return 0

    for p in broken:
        side = p.with_name(p.name + ".manifest")
        if side.exists():
            print("   %-16s already has %s, left alone" % (p.name, side.name))
            continue
        side.write_text(MANIFEST)
        print("   %-16s wrote %s" % (p.name, side.name))

    print("")
    print("Verify with a harmless invocation, e.g.:  dumpbin.exe /?")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
