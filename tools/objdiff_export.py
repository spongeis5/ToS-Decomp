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
from libmatch import trim_padding, pick_function
from match import can_shrink, can_extend
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

    # WHICH FUNCTIONS ARE LINKED is link.py's question; this file only writes
    # down its answer. Deciding it here would be the fifth tool in this
    # project to reimplement a comparison another tool owns.
    import link as linker
    complete_addrs, link_err = linker.complete_addresses()
    if link_err:
        complete_addrs = set()
        print("NOT MEASURED: %s" % link_err)
        print("  Every unit will be written `complete: false`, which is a")
        print("  floor and not a finding. The published report's linked")
        print("  figure will read zero until the link has actually run.")
        print("")

    # THE DENOMINATOR WE HAND objdiff-cli, summed as we hand it over.
    #
    # verify.py has to reconcile objdiff-cli's total_code against .text, and
    # modelling it from the inventory does not close: it came out 2,568 bytes
    # short, because the inventory OVERLAPS itself in places and because the
    # export reconciles a unit's recorded size with can_extend/can_shrink.
    # Both are real, and neither is knowable from outside this file. So write
    # down what was actually emitted instead of inferring it afterwards.
    emitted_sourced = 0

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
            # THE SHARED PICKER, because this was a fifth copy of the rule and
            # carried the same defect as matched_table's: mangled name, then
            # SUBSTRING, with no exact-name test between them. C symbols are
            # not mangled and `vorbis_book_decode` is a prefix of
            # `vorbis_book_decodev_add`, so four upstream rows resolved to
            # several functions each. Here that means the units objdiff-cli
            # diffs are the wrong ones, and its matched_code is what decomp.dev
            # publishes.
            got, why = pick_function(fns, want_sym)
            picked = [got] if got is not None else []
            if got is None:
                print("  %-28s %08X  %s" % (src.name, target, why))
        else:
            # No symbol column: take the LARGEST function, which is exactly
            # what match.py does in the same situation. Failing instead
            # dropped eight near-misses out of the export -- and a near-miss
            # missing from objdiff is the one thing objdiff is for. Rows in
            # attempts.txt rarely carry a symbol column, because an agent
            # appends them before anyone knows which function will matter.
            #
            # This is a fallback, not a guess about the manifest: build.py
            # still REFUSES an ambiguous manifest row, because there a wrong
            # pick would be spliced into .text.
            picked = fns
            if len(picked) > 1:
                picked = [max(fns, key=lambda f: len(f[1]))]
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

        # ... but the recorded extent can be TOO LONG. One .pdata unwind row
        # can cover a run of adjacent frameless functions, so a row may hold
        # several bodies (FINDINGS 7q). match.py and build.py both handle
        # that; without it here, seven verified matches exported as
        # `complete: false` and the unit list disagreed with verify.py --
        # a tool reporting a benign-looking failure it cannot actually see.
        #
        # Same proof as match.can_shrink, and it needs the relocation mask
        # rather than the raw bytes, so rebuild the mask from the relocs.
        mask = bytearray([1]) * len(code)
        for r in relocs:
            if r.off + 4 <= len(mask):
                mask[r.off:r.off + 4] = bytes(4)

        # ...and it can be TOO SHORT, which is the other half and was
        # missing. The inventory records BinAlloc at 8262F5D0 as 60 bytes;
        # the function is 136. match.py and build.py both grow it with
        # can_extend, so both call it a match -- while this exported a
        # 60-byte target against our correct 136-byte base and objdiff
        # showed 0.00%, with a `b 3c` in the target branching past its own
        # last instruction. Anyone reading that would go looking for a bug
        # in a function that has been correct for weeks.
        #
        # Both reconciliations, in the same order match.py applies them.
        # This is the fifth tool to decide an extent, and the rule stands:
        # import the shared predicates, do not re-derive the answer.
        grown = can_extend(img, inv, code, bytes(mask), target, tsize)
        if grown is not None:
            tbytes, tsize = grown, len(code)
        elif can_shrink(code, bytes(mask), tbytes, target, tsize):
            tbytes, tsize = tbytes[:len(code)], len(code)
        # Relocations are resolved against the overlapping prefix only.
        reloc_ref = tbytes[:len(code)] if tsize >= len(code) else             tbytes + bytes(len(code) - tsize)
        patched, _notes, _problems = buildmod.relocate(code, relocs, target,
                                                       reloc_ref, False)

        # A MATCH NEEDS AT LEAST ONE WORD THAT WAS NOT RELOCATED, and this has
        # to be decided BEFORE the base object is written, not just used to
        # label it afterwards. objdiff-cli does not read our categories to
        # compute matched_code -- it DIFFS these two files. So a base patched
        # into equality is counted as matched whatever we call it.
        #
        # Where every compared word carries a relocation, `relocate` copies
        # the image's bits into all of them and equality is guaranteed by
        # construction, having verified nothing. match.py has refused exactly
        # that since the can_shrink hole was found. 826C0FB8 is the function:
        # four bytes, one instruction, a tail call whose entire body is a
        # linker-supplied displacement. It is in attempts.txt, not the
        # manifest -- and this file still put its 4 bytes into the published
        # matched_code, because the two objects it wrote were identical.
        #
        # So write the base UNPATCHED there. The diff then shows what is
        # actually known about it, which is nothing.
        #
        # Found by the numerator being cross-checked three ways: build.py and
        # report.py said 35,072 and objdiff-cli said 35,076. Two counters
        # would have agreed and been wrong together.
        verified_word = any(mask[i:i + 4] != bytes(4)
                            for i in range(0, min(len(code), len(mask)), 4))

        # objdiff pairs symbols by NAME, so both sides use the same one. The
        # mangled name is unreadable in a UI, so use the address-tagged form.
        sym = "sub_%08X" % target
        unit = tag
        (OUT / "target" / (unit + ".o")).write_bytes(
            elf32_be_ppc_object(tbytes, sym))
        (OUT / "base" / (unit + ".o")).write_bytes(
            elf32_be_ppc_object(patched if verified_word else code, sym))
        # CATEGORIES, so objdiff's own progress bar tells the truth.
        #
        # A single percentage over every unit is dominated by the 818
        # generated stubs -- one expression each, written by script from
        # their own encodings -- and reads as far more of this game being
        # understood than is the case. objdiff groups and reports progress
        # per category, so the hand-written half gets its own number there
        # exactly as it does in MATCHED.md.
        #
        # `source_path` makes each unit click through to its .cpp, which is
        # the thing you want the moment a diff shows you something.
        # The split comes from tools/category.py, the one place that defines
        # it -- this file used to carry its own copy of the prefix tuple.
        import category as _cat
        kind = _cat.category(src)
        gen = (kind != "handwritten")
        cat = kind if (patched == tbytes and verified_word) else "nearmiss"
        units.append({
            "name": "%s (%s)" % (sym, src.stem),
            "target_path": "build/objdiff/target/%s.o" % unit,
            "base_path": "build/objdiff/base/%s.o" % unit,
            "metadata": {
                # `complete` MEANS LINKED, and this is the field that reaches
                # decomp.dev: `report.bin` is generated by objdiff-cli from
                # this file, and objdiff-cli sums complete_code over the units
                # whose metadata says complete.
                #
                # It used to be `patched == tbytes` -- matched. The published
                # report therefore claimed 34,096 bytes complete, i.e. LINKED,
                # at a time when nothing at all was linked and build.py said so
                # on every run. Matched and linked are different measures; the
                # schema carries both precisely because they are.
                #
                # tools/link.py owns the question. A function is complete when
                # it is inside a run link.exe laid out at its retail address
                # byte-identically, AND its source file defines nothing the
                # manifest does not name.
                "complete": target in complete_addrs,
                "source_path": str(src).replace("\\", "/"),
                "progress_categories": [cat],
                # Generated units are not worth a human's attention in a
                # visual diff; objdiff can fold them away.
                "auto_generated": gen,
            },
        })
        done += 1
        emitted_sourced += tsize

    # COVERAGE UNITS, so the report's denominator is the whole section.
    #
    # objdiff divides matched code by the total code of the units it is
    # given, and its model assumes those cover the binary -- a normal
    # project lists every translation unit, with the unwritten ones present
    # as target-only. Exporting only what we had sources for made the
    # denominator 39,684 bytes, and `objdiff-cli report generate` reported
    # 85.9% decompiled: the share of what has been WRITTEN that matches, not
    # progress. decomp.dev would have published that as a claim this game is
    # 86% done when it is 0.4%.
    #
    # tools/coverage.py buckets the rest of .text by address. Those are not
    # translation units and are named so nobody reads them as such; there is
    # no link map here and no way to recover file boundaries for code nobody
    # has read. They are target-only, so they contribute their bytes and
    # their function count to the denominator and nothing to the numerator.
    cov_dir = OUT / "target"
    for cov in sorted(cov_dir.glob("cov_*.o")):
        lo = cov.stem[4:]
        units.append({
            "name": "undecompiled/%s" % lo,
            "target_path": "build/objdiff/target/%s.o" % cov.stem,
            "metadata": {
                "complete": False,
                "progress_categories": ["undecompiled"],
                "auto_generated": True,
            },
        })

    cfg = {
        "min_version": "2.0.0",
        "custom_make": "python",
        "custom_args": ["tools/objdiff_export.py"],
        "build_target": False,
        "build_base": False,
        # attempts.txt too: a near-miss being edited is exactly when a
        # rebuild is wanted, and it was not in the watch list.
        "watch_patterns": ["src/*.cpp", "include/*.h", "src/manifest.txt",
                           "src/attempts.txt"],
        "progress_categories": [
            {"id": "handwritten", "name": "Hand-written from disassembly"},
            {"id": "generated", "name": "Generated from encodings"},
            {"id": "upstream", "name": "Upstream third-party source"},
            {"id": "nearmiss", "name": "Near-miss"},
            {"id": "undecompiled", "name": "No source yet"},
        ],
        "units": units,
    }
    CONFIG.write_text(json.dumps(cfg, indent=2) + "\n")

    # What we handed objdiff-cli, so verify.py can require its total_code to
    # equal it exactly rather than infer it from the inventory and be 2,568
    # bytes out. `emitted_coverage` comes from coverage.py's own buckets --
    # the same function that wrote the .o files listed above.
    import coverage as _cov
    _img, _bs = _cov.buckets()
    emitted_coverage = sum(s for _n, _l, fns in _bs for _a, s in fns)
    # The manifest DIGEST, not a timestamp. verify.py has to know whether the
    # objdiff-cli figures it reads still describe the current manifest, and
    # comparing mtimes fired on correct input: verify's own negative controls
    # restore src/manifest.txt byte for byte, which bumps its mtime, so every
    # run made the next run call a perfectly fresh report stale. A guard that
    # fires on correct input is worse than no guard -- it teaches you to read
    # past guards.
    import hashlib
    digest = hashlib.sha256(
        Path("src/manifest.txt").read_bytes()).hexdigest()[:16]
    (Path("build") / "objdiff_totals.json").write_text(json.dumps({
        "sourced_bytes": emitted_sourced,
        "coverage_bytes": emitted_coverage,
        "total_bytes": emitted_sourced + emitted_coverage,
        "sourced_units": done,
        "coverage_units": len(_bs),
        "manifest": digest,
    }, indent=2) + "\n")

    complete = sum(1 for u in units if u["metadata"]["complete"])
    print("exported %d unit(s), %d failed" % (done, failed))
    print("  target bytes handed to objdiff-cli: %d sourced + %d coverage"
          % (emitted_sourced, emitted_coverage))
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
