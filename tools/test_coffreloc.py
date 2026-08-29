"""Prove the relocation handling excuses the address and nothing else.

    python tools/test_coffreloc.py

The point of resolving relocations instead of masking them is that a wrong
REGISTER inside a relocated instruction must still be caught. `match.py`
blanks the whole 4-byte word and cannot catch it; `build.py` must.

The first version of build.py got this wrong in a way that would have passed
every test that only checked correct input: it treated IMAGE_REL_PPC_ADDR32NB
as a whole-word data patch, copying the retail word in wholesale -- register
fields included. Every function still verified. This is the test that would
have caught it, so it exists.

Each case takes a real compiled function, corrupts ONE field, and asserts
whether build.relocate still reproduces the target.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image
from libmatch import trim_padding
from coffreloc import functions_with_relocs
from build import compile_one, relocate

# (source, address, description) -- a function whose first two words are a
# relocated lis/addi pair addressing an extern.
CASE = (Path("src/global_field.cpp"), 0x82600BD8)


def load():
    blob, err = compile_one(CASE[0], "test_reloc")
    if blob is None:
        raise SystemExit("compile failed: %s" % err)
    name, code, relocs = max(functions_with_relocs(blob),
                             key=lambda f: len(f[1]))
    code, _m = trim_padding(code, b"\x01" * len(code))
    relocs = [r for r in relocs if r.off < len(code)]
    tbytes = Image().read(CASE[1], len(code))
    return name, code, relocs, tbytes


def run(label, code, relocs, tbytes, expect_ok):
    patched, _notes, problems = relocate(code, relocs, CASE[1], tbytes, False)
    ok = (patched == tbytes) and not problems
    verdict = "PASS" if ok == expect_ok else "FAIL"
    print("  %-4s %-52s %s" % (verdict, label,
                               "reproduces" if ok else "differs"))
    return ok == expect_ok


def main():
    name, code, relocs, tbytes = load()
    print("%s at %08X, %d bytes, %d relocation(s)\n"
          % (name, CASE[1], len(code), len(relocs)))
    print("  %-4s %-52s %s" % ("", "case", "result"))

    results = []
    results.append(run("unmodified source", code, relocs, tbytes, True))

    # Word 0 is `lis r11,<hi>` and carries ADDR32NB. Change the destination
    # register to r10 and leave everything else alone. The address field is
    # untouched, so only the register differs -- exactly what a whole-word
    # copy would hide.
    w0 = struct.unpack_from(">I", code, 0)[0]
    bad = bytearray(code)
    struct.pack_into(">I", bad, 0, (w0 & ~0x03E00000) | (10 << 21))
    results.append(run("destination register of the relocated `lis` changed",
                       bytes(bad), relocs, tbytes, False))

    # Word 1 is `addi rY,rX,<lo>` and carries SECREL. Change its SOURCE
    # register.
    w1 = struct.unpack_from(">I", code, 4)[0]
    bad = bytearray(code)
    struct.pack_into(">I", bad, 4, (w1 & ~0x001F0000) | (9 << 16))
    results.append(run("source register of the relocated `addi` changed",
                       bytes(bad), relocs, tbytes, False))

    # Changing the ADDRESS field itself must NOT fail: that field is supplied
    # by the linker and is legitimately taken from the image.
    bad = bytearray(code)
    struct.pack_into(">I", bad, 0, (w0 & ~0xFFFF) | 0x1234)
    results.append(run("address field of the `lis` changed (must be excused)",
                       bytes(bad), relocs, tbytes, True))

    # A non-relocated word must still be compared.
    bad = bytearray(code)
    w2 = struct.unpack_from(">I", code, 8)[0]
    struct.pack_into(">I", bad, 8, (w2 & ~0x03E00000) | (7 << 21))
    results.append(run("register in a NON-relocated word changed",
                       bytes(bad), relocs, tbytes, False))

    print("")
    print("%d of %d case(s) behaved as required." % (sum(results), len(results)))
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
