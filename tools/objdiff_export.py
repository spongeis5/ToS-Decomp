"""Export this project for objdiff, so the matching can be browsed visually.

    python tools/objdiff_export.py

objdiff compares two OBJECT FILES per unit: a `target` (what you are aiming
at) and a `base` (what you currently build). This project has neither shape
lying around -- the target is a linked retail image and the base is a COFF
object -- so both sides are synthesized here as PowerPC ELF relocatables:

    build/objdiff/target/<name>.o    the retail bytes at that address
    build/objdiff/base/<name>.o      our compiled bytes, relocations resolved
    objdiff.json                     the unit list objdiff reads

WHY ELF AND NOT COFF. Our real objects are COFF with machine 0x01F2
(IMAGE_FILE_MACHINE_POWERPCBE). objdiff's PowerPC support is built around the
ELF objects the GameCube/Wii projects use, so emitting ELF32 big-endian
EM_PPC is the form most likely to be understood. Nothing is lost: the bytes
are the same bytes, and the byte comparison that decides a match is
`tools/build.py`, not this.

WHY THE BASE HAS ITS RELOCATIONS ALREADY RESOLVED. A relocation's address is
chosen by the original linker and is not knowable from source, so build.py
reads each one back out of the image and patches it in. Emitting the base
un-patched would show every `bl` and every `lis`/`addi` pair as a difference
in objdiff even for a function that verifies perfectly. The resolved form is
what build.py actually checks, so it is what gets exported -- and the
relocated FIELD is the only thing taken from the image; opcode and register
fields are ours.

KNOWN LIMIT: objdiff will not decode VMX128. None of the currently matched
functions contain any (checked: 0 of 325 instructions), but the vector maths
this engine is full of will not render. `tools/ppcdis.py` is the decoder that
does know VMX128, and `tools/disasm.py` is the way to read those.
"""

import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import trim_padding
from coffreloc import functions_with_relocs
import build as buildmod

OUT = Path("build/objdiff")
CONFIG = Path("objdiff.json")

EM_PPC = 20
SHT_PROGBITS, SHT_SYMTAB, SHT_STRTAB = 1, 2, 3
SHF_ALLOC, SHF_EXECINSTR = 0x2, 0x4
STB_GLOBAL, STT_FUNC = 1, 2


def elf32_be_ppc_object(code, symbol):
    """A minimal ELF32 big-endian PowerPC relocatable holding one function.

    Sections: [0] null, [1] .text, [2] .symtab, [3] .strtab, [4] .shstrtab.
    Symbols:  [0] null, [1] `symbol` -> .text, size len(code), global func.
    """
    shstr = b"\0.text\0.symtab\0.strtab\0.shstrtab\0"
    off_text_name = shstr.index(b".text\0")
    off_symtab_name = shstr.index(b".symtab\0")
    off_strtab_name = shstr.index(b".strtab\0")
    off_shstr_name = shstr.index(b".shstrtab\0")

    sym_bytes = symbol.encode("utf-8")
    strtab = b"\0" + sym_bytes + b"\0"

    # Two symbol entries of 16 bytes: the null symbol and ours.
    symtab = bytes(16)
    symtab += struct.pack(">IIIBBH", 1, 0, len(code),
                          (STB_GLOBAL << 4) | STT_FUNC, 0, 1)

    ehsize, shentsize, shnum = 52, 40, 5
    off = ehsize
    text_off = off
    off += len(code)
    off = (off + 3) & ~3
    symtab_off = off
    off += len(symtab)
    strtab_off = off
    off += len(strtab)
    shstr_off = off
    off += len(shstr)
    off = (off + 3) & ~3
    shoff = off

    eh = b"\x7fELF" + bytes([1, 2, 1, 0]) + bytes(8)
    eh += struct.pack(">HHIIIIIHHHHHH",
                      1,            # e_type   ET_REL
                      EM_PPC,       # e_machine
                      1,            # e_version
                      0,            # e_entry
                      0,            # e_phoff
                      shoff,        # e_shoff
                      0,            # e_flags
                      ehsize, 0, 0, shentsize, shnum, 4)

    def sh(name, stype, flags, offset, size, link=0, info=0, align=1, ent=0):
        return struct.pack(">IIIIIIIIII", name, stype, flags, 0, offset,
                           size, link, info, align, ent)

    sections = b""
    sections += sh(0, 0, 0, 0, 0)
    sections += sh(off_text_name, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR,
                   text_off, len(code), align=4)
    sections += sh(off_symtab_name, SHT_SYMTAB, 0, symtab_off, len(symtab),
                   link=3, info=1, align=4, ent=16)
    sections += sh(off_strtab_name, SHT_STRTAB, 0, strtab_off, len(strtab),
                   align=1)
    sections += sh(off_shstr_name, SHT_STRTAB, 0, shstr_off, len(shstr),
                   align=1)

    blob = bytearray(shoff + len(sections))
    blob[0:len(eh)] = eh
    blob[text_off:text_off + len(code)] = code
    blob[symtab_off:symtab_off + len(symtab)] = symtab
    blob[strtab_off:strtab_off + len(strtab)] = strtab
    blob[shstr_off:shstr_off + len(shstr)] = shstr
    blob[shoff:shoff + len(sections)] = sections
    return bytes(blob)


def main(argv):
    rows, err = buildmod.read_manifest()
    if err:
        print(err)
        return 1
    # Also export the attempts that do NOT match. A unit list where every row
    # reads 100% shows nothing; the near-misses are the reason to open a
    # visual diff at all.
    n_manifest = len(rows)
    attempts = Path("src/attempts.txt")
    n_attempts = 0
    if attempts.exists():
        for line in attempts.read_text().splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            f = line.split()
            row, _e = buildmod.parse_manifest_line(line)
            rows.append(row)
            n_attempts += 1
    img = Image()
    inv = dict(load_inventory())

    (OUT / "target").mkdir(parents=True, exist_ok=True)
    (OUT / "base").mkdir(parents=True, exist_ok=True)

    units, done, failed = [], 0, 0
    for src, target, want_sym, unit_flags in rows:
        tag = "%s_%08X" % (src.stem, target)
        blob, cerr = buildmod.compile_one(src, tag, unit_flags)
        if blob is None:
            print("  %-28s COMPILE FAILED" % src.name)
            failed += 1
            continue
        fns = functions_with_relocs(blob)
        if want_sym:
            picked = [f for f in fns if ("?" + want_sym + "@@") in f[0]] or \
                     [f for f in fns if want_sym in f[0]]
        else:
            picked = fns
        if len(picked) != 1:
            print("  %-28s %08X  ambiguous symbol" % (src.name, target))
            failed += 1
            continue
        name, code, relocs = picked[0]
        code, _m = trim_padding(code, bytes([1]) * len(code))
        relocs = [r for r in relocs if r.off < len(code)]

        # The TARGET object must be the function's real extent, from the
        # inventory -- not our code's length. When ours is the wrong size
        # (sub_827FE808 compiles to 20 bytes against a 16-byte target) using
        # our length would splice four bytes of the NEXT function into the
        # target and show a difference that is not there.
        tsize = inv.get(target, len(code))
        tbytes = img.read(target, tsize)
        if tbytes is None or len(tbytes) != tsize:
            print("  %-28s %08X  unreadable" % (src.name, target))
            failed += 1
            continue
        # Relocations are resolved against the overlapping prefix only.
        reloc_ref = tbytes[:len(code)] if tsize >= len(code) else             tbytes + bytes(len(code) - tsize)
        patched, _notes, _problems = buildmod.relocate(code, relocs, target,
                                                       reloc_ref, False)

        # objdiff pairs symbols by NAME, so both sides use the same one. The
        # mangled name is unreadable in a UI, so use the address-tagged form.
        sym = "sub_%08X" % target
        unit = tag
        (OUT / "target" / (unit + ".o")).write_bytes(
            elf32_be_ppc_object(tbytes, sym))
        (OUT / "base" / (unit + ".o")).write_bytes(
            elf32_be_ppc_object(patched, sym))
        units.append({
            "name": "%s (%s)" % (sym, src.stem),
            "target_path": "build/objdiff/target/%s.o" % unit,
            "base_path": "build/objdiff/base/%s.o" % unit,
            "metadata": {"complete": patched == tbytes},
        })
        done += 1

    cfg = {
        "min_version": "2.0.0",
        "custom_make": "python",
        "custom_args": ["tools/objdiff_export.py"],
        "build_target": False,
        "build_base": False,
        "watch_patterns": ["src/*.cpp", "include/*.h", "src/manifest.txt"],
        "units": units,
    }
    CONFIG.write_text(json.dumps(cfg, indent=2) + "\n")

    complete = sum(1 for u in units if u["metadata"]["complete"])
    print("exported %d unit(s), %d failed" % (done, failed))
    print("  from src/manifest.txt (verified matches): %d" % n_manifest)
    print("  from src/attempts.txt (not matching yet): %d" % n_attempts)
    print("  complete (base == target): %d" % complete)
    print("  -> %s" % CONFIG)
    print("")
    print("Open the folder in objdiff and it will list every unit. `complete`")
    print("marks the ones build.py verifies byte for byte.")
    print("")
    print("objdiff will NOT decode VMX128 -- none of these functions contain")
    print("any, but the engine's vector maths will not render. Use")
    print("tools/disasm.py for those.")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
