"""Re-derive the blocked-run external-symbol analysis for the whole-image link.

FINDINGS 7y recorded, over the 26 blocked runs: 79 references, of which 39
resolve to a .text function start, 20 to .data, 14 to .rdata, 3 land in .text
at something that is NOT a function start, and 3 could not be solved from the
retail word at all. The last two classes are the loose ends the whole-image
link was told to settle first, and the numbers were measured against an older
manifest.

This re-derives the list from the CURRENT manifest using the repo's own
machinery -- link.describe() for the needs_address sets and
coffreloc.solve_address() for the resolution -- so the classification cannot
drift from the tools, and writes each loose-end class to a file with enough
context to investigate without re-running anything.

    python tools/loose_ends.py          summary + build/loose_ends/*.txt

**BOTH LOOSE ENDS ARE NOW SETTLED**, over 167 symbols needed by 52 blocked
runs, 165 addresses recovered:

  * of the 3 that land in .text at a NON-START, **2 are XEX import thunks**.
    `LockEnter` and `LockLeave` are our invented names for `8291284C` and
    `8291285C`, which are `mtctr r11 ; bctr` behind two ordinal words, and
    `build/imports.txt` has named them all along:
    `xboxkrnl.exe!RtlEnterCriticalSection` and `!RtlLeaveCriticalSection`.
    They are imports, not missing functions. **1** genuine non-start is
    left: a data object at `827A7C88`, reached by an ADDR32NB/SECREL pair
    from inside .text.
  * the ones that "could not be solved" are **0**. The two that looked
    unsolvable are `__declspec(thread)` variables at TLS slot 40, reached
    through r13 by a TOCREL14 -- an answer, not a failure. See below.

(An earlier draft of this docstring said those two were "past the last
inventory row". They are not: the last row ends at 82923098 and both sit
below it, in the import-thunk region just under BINK. Checked, and wrong.)

FIVE DEFECTS WERE FIXED IN THE FIRST VERSION. The second is the one worth
remembering; the fourth and fifth are the ones that changed the conclusion,
and they are the same mistake twice -- a thing the repository already knew,
reported as a thing nobody knows:

  * `classify()` called a bare `section_of(a)`. There is no module-level
    `section_of` in peimage -- it is a METHOD on `Image` -- so the tool
    raised NameError before printing anything. Fatal, and therefore
    harmless.

  * `pending_hi = {}` was INSIDE the loop over relocations, so it was
    cleared before every one. A REFLO could never see the REFHI recorded on
    the previous iteration, and every hi/lo pair was reported as
    "low half, no REFHI seen". Measured against the corrected version on the
    same manifest: 111 fabricated unsolved sites, 96 addresses recovered
    instead of 165, and 71 of 167 symbols looking unresolvable when
    exactly 2 are. That is the dangerous shape -- it does not crash, it
    answers, and the answer is the one this tool exists to produce.

  * the relocations were walked in COFF order. build.py sorts them by
    offset before pairing, because a REFHI must be seen before its REFLO
    and nothing guarantees the object lists them that way. It changes
    nothing on today's data -- both orders give 165 -- which is exactly what
    a latent bug looks like while it waits.

  * a TLS slot was filed under "unsolved". `coffreloc.solve_address` returns
    `('tls', offset)` as its own kind, and says why in its docstring: "so it
    is not printed as though some location had been recovered". Folding that
    back into "could not be solved" turned two fully-explained
    `__declspec(thread)` references into the mystery this tool was written to
    investigate. A measured fact reported as an absence -- the same shape as
    every other defect this repository keeps finding, and the reason
    UNSOLVED now means what it says.

  * the XEX import thunks were classified as "text non-start", which is
    true and useless. `build/imports.txt` names every one of them, so two
    of the three remaining loose ends were answered in a file this tool was
    not reading. Same lesson as the TLS one, one file over.

The pairing below is a COPY of build.py's rule (tools/build.py, `patch()`).
It is written out rather than imported because `patch()` also rewrites the
code bytes and returns notes keyed by offset, and this needs neither. If the
rule ever changes, it has to change in both -- which is the drift this
project has paid for repeatedly, so the better fix is to lift the pairing
into coffreloc.py and have both call it.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import link
import matched_table
import switchtab
from coffreloc import COMPANION, WHOLE_WORD, type_name, solve_address
from peimage import Image, load_inventory

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build/loose_ends"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    img = Image()
    inv = dict(load_inventory())

    # THE XEX IMPORT THUNKS, which are the other thing in .text that is not a
    # function of ours. Two of the three "text non-start" symbols were
    # 8291284C and 8291285C -- `mtctr r11 ; bctr` behind two ordinal words --
    # and build/imports.txt has named them all along:
    # xboxkrnl.exe!RtlEnterCriticalSection and !RtlLeaveCriticalSection. Our
    # LockEnter/LockLeave are invented names for XDK kernel imports.
    #
    # Reporting them as an unclassified non-start made two fully-known
    # references look like an open question, which is the same mistake the
    # TLS one was.
    imports = {}
    imp = ROOT / "build/imports.txt"
    if imp.exists():
        for line in imp.read_text().splitlines():
            line = line.split("#")[0].strip()
            f = line.split()
            if len(f) >= 4:
                try:
                    imports[int(f[0], 16)] = "%s!%s" % (f[1], f[3])
                except ValueError:
                    pass

    # ONE reader for the jump tables, tools/switchtab.py. The first version
    # parsed build/switch_tables.txt here as well -- a sixth copy of that
    # reader, and the sixth to handle a recorded length of 0 its own way. It
    # assumed the unknown extent was 4 bytes; switchtab MEASURES it as the
    # run of aligned .text addresses at the base. 106 of 437 entries record
    # no length, so the assumption was not a corner case.
    tables = switchtab.Tables(img)
    if tables.missing:
        print("build/switch_tables.txt is missing -- run tools/switches.py.")
        print("Without it a jump table cannot be told from a non-start, and")
        print("that distinction is the whole point of the nonstart class.")
        return 1

    sized = link.compiled(matched_table.rows())
    runs = link.runs_of(sized)

    # describe() compiles and links; calling it twice per run doubled the
    # work for a summary line. Once, kept.
    described = [(run, link.describe(run)) for run in runs]
    nerr = sum(1 for _r, d in described if d[4])
    nblocked = sum(1 for _r, d in described if not d[4] and d[2])

    # symbol -> {"runs": [first addr of each run needing it],
    #            "sites": [(retail site addr, rtype, fn addr)],
    #            "resolved": set of addresses,
    #            "unsolved": list of (site, type, why)}
    info = {}
    unreadable = 0
    for run, (_span, _placed, needs, _nofix, err) in described:
        if err or not needs:
            continue
        for addr, _size, src, sym, flags in run:
            blob, _err = link.obj_for(src, flags)
            if blob is None:
                continue
            got = link.pick(blob, sym)
            if got is None:
                continue
            _name, _code, relocs = got

            # PER FUNCTION, and sorted by offset -- build.py's rule exactly.
            pending_hi = {}
            for r in sorted(relocs, key=lambda x: x.off):
                if r.sym not in needs or r.type in COMPANION:
                    continue
                site = addr + r.off
                raw = img.read(site, 4)
                if raw is None:
                    # Not backed by the image. Counted and reported rather
                    # than crashing in struct.unpack_from on None, and
                    # rather than skipped silently -- a site nobody could
                    # read is not a site with nothing at it.
                    unreadable += 1
                    continue
                theirs = struct.unpack_from(">I", raw, 0)[0]
                rec = info.setdefault(
                    r.sym, {"runs": [], "sites": [], "resolved": set(),
                            "tls": set(), "unsolved": []})
                if run[0][0] not in rec["runs"]:
                    rec["runs"].append(run[0][0])
                rec["sites"].append((site, r.type, addr))

                if r.type in WHOLE_WORD:
                    rec["resolved"].add(theirs)
                    continue
                solved = solve_address(r.type, theirs, site)
                if solved is None:
                    rec["unsolved"].append((site, r.type, "type not solved"))
                elif solved[0] == "tls":
                    # A TLS SLOT IS AN ANSWER, NOT A FAILURE. The first
                    # version filed these under "unsolved", which is how the
                    # two remaining loose ends came to look like a mystery:
                    # both are `__declspec(thread)` variables at slot 40,
                    # reached through r13 by an IMAGE_REL_PPC_TOCREL14 that
                    # MATCHED.md documents and that never folds with
                    # anything. coffreloc.solve_address returns 'tls' as its
                    # own kind precisely "so it is not printed as though
                    # some location had been recovered" -- and this took
                    # that deliberate distinction and threw it away.
                    #
                    # There is no address to recover and nothing is missing.
                    # Reported as its own class so the UNSOLVED count means
                    # what it says.
                    rec["tls"].add(solved[1])
                elif solved[0] == "abs":
                    rec["resolved"].add(solved[1])
                elif solved[0] == "hi":
                    pending_hi[r.sym] = solved[1]
                else:
                    hi = pending_hi.get(r.sym)
                    if hi is None:
                        rec["unsolved"].append(
                            (site, r.type, "low half, no REFHI seen"))
                    else:
                        rec["resolved"].add((hi + solved[1]) & 0xFFFFFFFF)

    def classify(sym, rec):
        if not rec["resolved"] and rec["tls"] and not rec["unsolved"]:
            return "TLS slot (no address)"
        if rec["unsolved"] and not rec["resolved"]:
            return "UNSOLVED"
        if not rec["resolved"]:
            return "NO-RESOLUTION"
        classes = set()
        for a in rec["resolved"]:
            sec = img.section_of(a)          # a METHOD on Image, not a global
            if a in imports:
                classes.add("XEX import thunk")
            elif sec and sec.startswith(".text") and a in inv:
                classes.add("text function start")
            elif sec and sec.startswith(".text"):
                classes.add("jump table" if a in tables else "text non-start")
            elif sec:
                classes.add(sec)
            else:
                classes.add("?? %08X not in any section" % a)
        return " + ".join(sorted(classes))

    cls_of = dict((sym, classify(sym, rec)) for sym, rec in info.items())
    rows = sorted(info.items(),
                  key=lambda kv: (cls_of[kv[0]],
                                  min(kv[1]["resolved"])
                                  if kv[1]["resolved"] else 0xFFFFFFFF))
    by_class = {}
    for sym, _rec in rows:
        by_class.setdefault(cls_of[sym], []).append(sym)

    print("%d run(s) of 2+, %d blocked on needs_address, %d blocked on error"
          % (len(runs), nblocked, nerr))
    print("%d distinct symbol(s) needed by blocked runs, %d address(es) "
          "recovered:\n"
          % (len(info), sum(len(r["resolved"]) for r in info.values())))
    for cls in sorted(by_class):
        print("%-28s %3d of %d" % (cls, len(by_class[cls]), len(info)))
    if unreadable:
        print("")
        print("%d site(s) are not backed by the image and were not read."
              % unreadable)
    print("")

    tsv = OUT / "symbols.tsv"
    with tsv.open("w", encoding="utf-8", newline="\n") as f:
        f.write("symbol\tn_runs\tn_sites\ttypes\tresolved\tclassification\n")
        for sym, rec in rows:
            types = ",".join(sorted(set(type_name(t)
                                        for _s, t, _f in rec["sites"])))
            resolved = " ".join("%08X" % a
                                for a in sorted(rec["resolved"])) or "-"
            f.write("%s\t%d\t%d\t%s\t%s\t%s\n"
                    % (sym, len(rec["runs"]), len(rec["sites"]), types,
                       resolved, cls_of[sym]))

    for name, want in (("nonstart", "text non-start"),
                       ("import", "XEX import thunk"),
                       ("tls", "TLS slot"),
                       ("unsolved", "UNSOLVED")):
        p = OUT / ("%s.txt" % name)
        with p.open("w", encoding="utf-8", newline="\n") as f:
            picked = [kv for kv in rows if want in cls_of[kv[0]]]
            f.write("%d symbol(s) in class %r, of %d needed by blocked runs\n\n"
                    % (len(picked), want, len(info)))
            for sym, rec in picked:
                f.write("SYMBOL %s\n" % sym)
                f.write("  needed by run(s) starting: %s\n"
                        % " ".join("%08X" % a for a in sorted(rec["runs"])))
                for site, rtype, fn in sorted(rec["sites"]):
                    raw = img.read(site, 4)
                    word = struct.unpack_from(">I", raw, 0)[0] if raw else None
                    f.write("  site %08X (in fn %08X) %s retail word %s\n"
                            % (site, fn, type_name(rtype),
                               "%08X" % word if word is not None
                               else "UNREADABLE"))
                if rec["resolved"]:
                    f.write("  resolved: %s\n"
                            % " ".join("%08X" % a
                                       for a in sorted(rec["resolved"])))
                named = [(a, imports[a]) for a in sorted(rec["resolved"])
                         if a in imports]
                for a, nm in named:
                    f.write("  %08X is the XEX import thunk for %s\n"
                            % (a, nm))
                if rec["tls"]:
                    f.write("  TLS slot offset(s): %s  (read through r13; "
                            "not an address)\n"
                            % " ".join(str(n) for n in sorted(rec["tls"])))
                for site, rtype, why in rec["unsolved"]:
                    f.write("  unsolved at %08X (%s): %s\n"
                            % (site, type_name(rtype), why))
                f.write("\n")
        print("wrote %s (%d symbol(s))"
              % (p, len([kv for kv in rows if want in cls_of[kv[0]]])))
    print("wrote %s (%d symbol(s))" % (tsv, len(rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
