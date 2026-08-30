"""How much of the matched byte total did the COMPILER actually produce?

    python tools/evidence.py            the split, with denominators
    python tools/evidence.py --worst    the functions with the least evidence

`bytes rebuilt from source` counts every byte of every matched function. Not
all of those bytes are the same KIND of evidence, and the difference is not
small.

A relocated word is one the compiler emitted with a hole in it: `bl 0`,
`lis r11,0`, `addi r10,r11,0`. The opcode, the register fields and the
addressing mode are the compiler's -- `build.py` resolves relocations rather
than masking them, so a wrong register inside one IS caught. But the field
itself is not ours in a splice: `build.py` takes it from the retail word.

So there are three kinds of byte behind one number:

  COMPILER      a whole word the compiler produced, compared against the
                image and equal. The only kind that is evidence with no
                caveat at all.
  LINKED        a relocated field inside a run that `link.py` handed to the
                retail link.exe, which computed it from OUR placement of OUR
                symbols and got the image's bytes. Derived, not copied --
                this is exactly what the real link buys.
  SPLICED       a relocated field that no linked run covers, so `build.py`
                took it out of the retail word. The instruction is
                reproduced; the address in it is copied.

None of this makes a match wrong. `match.py` refuses a function whose every
word is relocated, precisely because such a function is not confirmed by
comparison at all -- this is the same question asked of the whole corpus
instead of one function at a time, and asked in bytes rather than in
functions.

The point is to be able to say which fraction of the headline is which,
before someone else works it out.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import link
import matched_table
from libmatch import coff_functions, pick_function, trim_padding

ROOT = Path(__file__).resolve().parent.parent


def rows():
    """Manifest rows as (src, addr, sym, flags)."""
    return matched_table.rows()


def analyse():
    """-> (per-function list, totals dict, [failures])."""
    linked, why = link.linked_addresses()
    if linked is None:
        return None, None, ["link.py has not been run: %s" % why]

    out, fails = [], []
    for src, addr, sym, flags in rows():
        blob, err = link.obj_for(src, flags)
        if blob is None:
            fails.append((src, addr, err or "did not compile"))
            continue
        fns = coff_functions(blob)
        got, whynot = pick_function(fns, sym)
        if got is None:
            fails.append((src, addr, whynot))
            continue
        name, code, mask = got[0], got[1], got[2]
        code, mask = trim_padding(code, mask)
        nwords = len(code) // 4
        reloc = 0
        for i in range(nwords):
            if not all(mask[i * 4:i * 4 + 4]):
                reloc += 1
        out.append({"src": src, "addr": addr, "name": name,
                    "words": nwords, "reloc": reloc,
                    "linked": addr in linked})
    return out, None, fails


def main(argv):
    per, _t, fails = analyse()
    if per is None:
        for f in fails:
            print(f)
        print("")
        print("Refusing to report a split measured against no link. A")
        print("relocated word's provenance depends on whether a linked run")
        print("covers it, and without build/linked.txt that is unknown --")
        print("not zero.")
        return 1

    compiler = sum((r["words"] - r["reloc"]) * 4 for r in per)
    linkedb = sum(r["reloc"] * 4 for r in per if r["linked"])
    spliced = sum(r["reloc"] * 4 for r in per if not r["linked"])
    total = compiler + linkedb + spliced
    nfn = len(per)

    print("%d of %d manifest row(s) measured; %d could not be read"
          % (nfn, nfn + len(fails), len(fails)))
    for src, addr, why in fails[:5]:
        print("    %-40s %08X  %s" % (src, addr, str(why)[:40]))
    print("")
    print("%s byte(s) of matched code, by what produced each word:" %
          "{:,}".format(total))
    print("")
    print("  COMPILER  %9s   %5.2f%%   whole words the compiler emitted,"
          % ("{:,}".format(compiler), 100.0 * compiler / total))
    print("                                  compared against the image")
    print("  LINKED    %9s   %5.2f%%   relocated fields link.exe computed"
          % ("{:,}".format(linkedb), 100.0 * linkedb / total))
    print("                                  from our own placement")
    print("  SPLICED   %9s   %5.2f%%   relocated fields build.py took"
          % ("{:,}".format(spliced), 100.0 * spliced / total))
    print("                                  out of the retail word")
    print("")
    derived = compiler + linkedb
    print("%s byte(s) (%.2f%%) owe nothing to the image's own bytes."
          % ("{:,}".format(derived), 100.0 * derived / total))
    print("%s byte(s) (%.2f%%) reproduce the instruction and copy the field."
          % ("{:,}".format(spliced), 100.0 * spliced / total))
    print("")
    print("Every byte above is byte-identical to retail either way; this is")
    print("about what produced it, not whether it is right.")

    # WHY `LINKED` IS SO SMALL, WHICH IS THE POINT OF THIS TOOL.
    #
    # A run links only when every symbol it references is inside it --
    # link.py reports the rest as "needs an address for N symbol(s)". So the
    # runs that link are, by construction, the runs with the least for a
    # linker to do, and the relocation-heavy code sits in the runs that
    # cannot link. The real link is doing the EASY half of the resolution
    # question while proving the HARD half of the layout question.
    lin = [r for r in per if r["linked"]]
    unl = [r for r in per if not r["linked"]]
    lw = sum(r["words"] for r in lin)
    lr = sum(r["reloc"] for r in lin)
    uw = sum(r["words"] for r in unl)
    ur = sum(r["reloc"] for r in unl)
    print("")
    print("WHERE THE RELOCATIONS ARE, and why LINKED is the small number:")
    print("")
    print("  in a linked run      %4d of %5d word(s) relocated   %5.2f%%"
          % (lr, lw, 100.0 * lr / lw if lw else 0.0))
    print("  not in one          %5d of %5d word(s) relocated   %5.2f%%"
          % (ur, uw, 100.0 * ur / uw if uw else 0.0))
    print("")
    print("A run links only when every symbol it references is inside it.")
    print("So the runs that link are the ones with least for a linker to do,")
    print("and the relocation-dense code is exactly what cannot link yet.")
    if lr:
        top = max(lin, key=lambda r: r["reloc"])
        print("")
        print("  %s of the %d linked relocated word(s) are in ONE function,"
              % (top["reloc"], lr))
        print("  %08X %s." % (top["addr"], Path(top["src"]).name))
    print("")
    print("This does not diminish the link: its job is PACKING, ORDER and")
    print("PADDING, and it proves those over every byte of every linked run.")
    print("It does bound what the link has settled about RELOCATION, and")
    print("that bound is the case for linking the whole image at once.")

    if "--worst" in argv:
        print("")
        print("Functions with the least compiler-produced evidence, by")
        print("fraction of words relocated (>= 50%, largest first):")
        print("")
        worst = [r for r in per if r["words"] and
                 r["reloc"] * 2 >= r["words"]]
        worst.sort(key=lambda r: (-(r["reloc"] / float(r["words"])),
                                  -r["words"]))
        if not worst:
            print("  none -- no matched function is half relocated.")
        for r in worst[:25]:
            print("  %08X  %-30s %3d of %3d word(s) relocated%s"
                  % (r["addr"], Path(r["src"]).name, r["reloc"], r["words"],
                     "  [in a linked run]" if r["linked"] else ""))
        print("")
        print("  %d of %d matched function(s) are at least half relocated."
              % (len(worst), nfn))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
