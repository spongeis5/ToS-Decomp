"""Target-only objdiff units for the code with no source yet.

    python tools/coverage.py            report what it would emit
    python tools/coverage.py --write    write build/objdiff/target/cov_*.o

WHY THIS EXISTS, and it is the difference between a true headline number
and a false one.

objdiff's progress report divides matched code by the total code of the
units it is given. Its model assumes those units COVER THE WHOLE BINARY:
a normal decompilation project has a link map or a splits file naming
every translation unit, and the ones nobody has written yet appear as
target-only units -- present in the denominator, zero in the numerator.

This project exported only the units it had sources for, so the denominator
was the 39,684 bytes already written and `objdiff-cli report generate` said
**85.9% decompiled**. That is the fraction of what has been written that
matches. Reported as project progress it claims this game is 86% done when
it is 0.4%, and decomp.dev would publish exactly that number.

So the rest of `.text` has to be in the report too. There is no link map
here, and no way to recover the original translation-unit boundaries for
code nobody has read -- so the undecompiled code is grouped into synthetic
units by ADDRESS RANGE. That is not a claim about how the game was
organised; it is a bucket, named as one, and it puts the right number of
bytes in the denominator.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build/objdiff/target"
BUCKET = 0x20000          # 128 KiB per synthetic unit


def sourced():
    done = set()
    for name in ("manifest.txt", "attempts.txt"):
        p = ROOT / "src" / name
        if not p.exists():
            continue
        for line in p.read_text(encoding="utf-8").splitlines():
            line = line.split("#")[0].strip()
            if not line:
                continue
            f = line.split()
            if len(f) >= 2:
                try:
                    done.add(int(f[1], 16))
                except ValueError:
                    pass
    return done


def elf_multi(code, syms):
    """ELF32 BE PPC relocatable, one .text and N function symbols.

    `syms` is [(name, offset, size)]. The single-symbol writer in
    objdiff_export.py cannot express a bucket holding hundreds of functions,
    and objdiff counts FUNCTIONS as well as bytes -- a bucket with one
    symbol would put the byte count in the denominator but hide how many
    functions it stands for.
    """
    shstr = b"\0.text\0.symtab\0.strtab\0.shstrtab\0"
    n_text = shstr.index(b".text\0")
    n_symtab = shstr.index(b".symtab\0")
    n_strtab = shstr.index(b".strtab\0")
    n_shstr = shstr.index(b".shstrtab\0")

    strtab = bytearray(b"\0")
    entries = []
    for name, off, size in syms:
        entries.append((len(strtab), off, size))
        strtab += name.encode("utf-8") + b"\0"

    symtab = bytearray(bytes(16))
    for nameoff, off, size in entries:
        symtab += struct.pack(">IIIBBH", nameoff, off, size,
                              (1 << 4) | 2, 0, 1)     # GLOBAL FUNC, shndx 1

    ehsize, shentsize, shnum = 52, 40, 5
    o = ehsize
    text_off = o
    o += len(code)
    o = (o + 3) & ~3
    symtab_off = o
    o += len(symtab)
    strtab_off = o
    o += len(strtab)
    shstr_off = o
    o += len(shstr)
    o = (o + 3) & ~3
    shoff = o

    eh = b"\x7fELF" + bytes([1, 2, 1, 0]) + bytes(8)
    eh += struct.pack(">HHIIIIIHHHHHH", 1, 20, 1, 0, 0, shoff, 0,
                      ehsize, 0, 0, shentsize, shnum, 4)

    def sh(name, stype, flags, offset, size, link=0, info=0, align=1, ent=0):
        return struct.pack(">IIIIIIIIII", name, stype, flags, 0, offset,
                           size, link, info, align, ent)

    sections = (sh(0, 0, 0, 0, 0)
                + sh(n_text, 1, 0x2 | 0x4, text_off, len(code), align=4)
                + sh(n_symtab, 2, 0, symtab_off, len(symtab),
                     link=3, info=1, align=4, ent=16)
                + sh(n_strtab, 3, 0, strtab_off, len(strtab), align=1)
                + sh(n_shstr, 3, 0, shstr_off, len(shstr), align=1))

    blob = bytearray(shoff + len(sections))
    blob[0:len(eh)] = eh
    blob[text_off:text_off + len(code)] = code
    blob[symtab_off:symtab_off + len(symtab)] = symtab
    blob[strtab_off:strtab_off + len(strtab)] = strtab
    blob[shstr_off:shstr_off + len(shstr)] = shstr
    blob[shoff:shoff + len(sections)] = sections
    return bytes(blob)


def buckets():
    """-> [(name, lo, [(addr, size)])] for undecompiled .text functions."""
    img = Image()
    inv = sorted(load_inventory())
    text = next(s for s in img.sections if s["name"] == ".text")
    tlo = text["va"]
    thi = tlo + (text["vsize"] or text["rawsz"])
    done = sourced()

    groups = {}
    for addr, size in inv:
        if not (tlo <= addr < thi) or size <= 0 or addr in done:
            continue
        key = (addr - tlo) // BUCKET
        groups.setdefault(key, []).append((addr, size))
    out = []
    for key in sorted(groups):
        lo = tlo + key * BUCKET
        out.append(("undecompiled/%08X" % lo, lo, groups[key]))
    return img, out


def main(argv):
    img, bs = buckets()
    total = sum(sum(s for _a, s in fns) for _n, _l, fns in bs)
    nfn = sum(len(fns) for _n, _l, fns in bs)
    print("%d synthetic unit(s) of %d KiB, covering %d function(s) and"
          % (len(bs), BUCKET // 1024, nfn))
    print("%d byte(s) of .text that have no source yet." % total)
    print("")
    print("These are BUCKETS BY ADDRESS, not translation units. There is no")
    print("link map here and no way to recover the original file boundaries")
    print("for code nobody has read. They exist so the report's denominator")
    print("is the whole section rather than only what has been written.")

    if "--write" not in argv:
        print("")
        print("nothing written; pass --write")
        return 0

    OUT.mkdir(parents=True, exist_ok=True)
    written = 0
    for name, lo, fns in bs:
        first = fns[0][0]
        last = fns[-1][0] + fns[-1][1]
        blob = img.read(first, last - first)
        if blob is None or len(blob) != last - first:
            print("  %s unreadable -- skipped" % name)
            continue
        syms = [("sub_%08X" % a, a - first, s) for a, s in fns]
        path = OUT / ("cov_%08X.o" % lo)
        path.write_bytes(elf_multi(blob, syms))
        written += 1
    print("")
    print("wrote %d target object(s) to %s" % (written, OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
