"""Disassemble the whole of .text to a file.

    python tools/dumptext.py            -> build/text_dis.txt

About 83 MB and a couple of minutes. Only one thing needs it -- the image
census in `tools/vmx128_intrinsics.py`, which asks which VMX128 forms are
actually present so it can check every one is reachable from some XDK
intrinsic.

This exists because nothing regenerated that file. It was produced once, by
hand, and the command was never written down; a fresh clone had no way to
make it. `vmx128_intrinsics.py` then degraded to reporting ZERO forms in the
image, which reads exactly like a fact about the image rather than a missing
input -- the failure this project has a rule about. That tool now refuses
instead, and this is what satisfies it.

Uses `tools/ppcdis.py`, the only decoder here that knows VMX128. Capstone
would silently drop 2% of the instructions, and this file exists specifically
to count instructions capstone cannot see.
"""

import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image
import ppcdis

OUT = Path("build/text_dis.txt")
CHUNK = 8192


def main(argv):
    if not ppcdis.available():
        print("build/ppcdis.exe is missing, and the whole point of this file")
        print("is to count VMX128 forms that only it can decode. Build it")
        print("first -- see 'Rebuilding from scratch' in README.md.")
        return 1

    img = Image()
    text = next((s for s in img.sections if s["name"] == ".text"), None)
    if text is None:
        print("no .text section")
        return 1
    base = text["va"]
    size = text["vsize"] or text["rawsz"]
    data = img.read(base, size)
    if data is None or len(data) != size:
        print("could not read .text")
        return 1

    n = size // 4
    print("disassembling .text %08X..%08X, %d instruction(s)"
          % (base, base + size, n))
    t0 = time.time()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    written = 0
    with OUT.open("w") as f:
        for off in range(0, n, CHUNK):
            count = min(CHUNK, n - off)
            words = struct.unpack_from(">%dI" % count, data, off * 4)
            va = base + off * 4
            for a, w, text_ in ppcdis.words(list(words), va):
                f.write("%08X %08x %s\n" % (a, w, text_.strip()))
                written += 1
            if off % (CHUNK * 32) == 0 and off:
                print("  %7d / %d" % (off, n))

    if written != n:
        print("WROTE %d line(s) for %d instruction(s) -- refusing to leave a"
              % (written, n))
        print("short file, because a truncated census reads as a small count")
        print("rather than as a missing input.")
        OUT.unlink()
        return 1
    print("  %d line(s) in %.0fs -> %s (%.0f MB)"
          % (written, time.time() - t0, OUT, OUT.stat().st_size / 1e6))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
