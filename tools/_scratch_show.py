"""Compile one variant from a variants file and print its disassembly beside
the target's, so a shape that scored badly can be read rather than guessed at.

    python tools/_scratch_show.py <variants.py> <address> <variant-name-substr> [--os]

Scratch helper for the near-miss session. Not part of the build.
"""
import importlib.util
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import coff_functions, trim_padding
from match import parse_flags
import ppcdis
import xdkcc

XDK = Path("SDKFiles/xdk/XDK")
WORK = Path("build/permute")
FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]


def main(argv):
    spec = importlib.util.spec_from_file_location("variants", argv[1])
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    target = int(argv[2], 16)
    want = argv[3].lower()
    flags = parse_flags("/O2,/Os,/Gy,/GS-,/fp:fast") if "--os" in argv else list(FLAGS)

    img = Image()
    sizes = dict(load_inventory())
    tsize = sizes[target]
    tbytes = img.read(target, tsize)

    for name, textsrc in mod.BODIES:
        if want not in name.lower():
            continue
        WORK.mkdir(parents=True, exist_ok=True)
        src = WORK / "s.cpp"
        src.write_text(textsrc)
        blob, err = xdkcc.compile_obj(src, WORK / "s.obj", flags, WORK)
        if blob is None:
            print("%s: DID NOT COMPILE: %s" % (name, (err or "")[:400]))
            continue
        fns = coff_functions(blob)
        sym, code, mask = max(fns, key=lambda f: len(f[1]))
        code, mask = trim_padding(code, mask)
        print("=== %s   (%s)  %d B, target %d B" % (name, sym, len(code), tsize))
        n = max(len(code), tsize) // 4
        for i in range(n):
            va = target + i * 4
            a = struct.unpack_from(">I", tbytes, i * 4)[0] if i * 4 + 4 <= tsize else None
            b = struct.unpack_from(">I", code, i * 4)[0] if i * 4 + 4 <= len(code) else None
            ta = ppcdis.words([a], va)[0][2] if a is not None else "-"
            tb = ppcdis.words([b], va)[0][2] if b is not None else "-"
            reloc = b is not None and not all(mask[i * 4:i * 4 + 4])
            flag = "r" if reloc else (" " if a == b else "X")
            print("  %08X %s want %-32s got %s" % (va, flag, ta, tb))
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
