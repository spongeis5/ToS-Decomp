"""The matching loop: compile a candidate, compare it against the retail bytes.

    python tools/match.py src/grid_indices.cpp 822607F0

Compiles with the XDK's own cl.exe (15.00.8153), pulls the code the compiler
emitted, and diffs it instruction by instruction against the image.

Notes that cost time to learn:

  * cl.exe is invoked through subprocess directly, NOT through a shell.  Git
    Bash rewrites MSVC-style `/c` and `/nologo` into Windows paths and the
    flags are silently dropped.
  * A COMDAT section is padded; the trailing nops/zeros are trimmed before
    comparison, and what was trimmed is reported.
  * Relocated words are marked.  An object refers to symbols by placeholder,
    so a difference in a relocated word is EXPECTED and is shown separately
    from a real mismatch -- counting them together would make a correct
    function look wrong.

Exit status is 0 only on an exact match of the non-relocated bytes.
"""

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import coff_functions, trim_padding

XDK = Path("SDKFiles/xdk/XDK")
CL = XDK / "bin/win32/cl.exe"
INCLUDE = XDK / "include/xbox"

DEFAULT_FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]

import ppcdis
import xdkcc


def text(word, va):
    # One decoder for both sides. Capstone cannot read VMX128, and a diff
    # where one side shows `.long` is not a comparison of instructions.
    return ppcdis.words([word], va)[0][2]


UNCOND = (0x4E800020, 0x4E800420)          # blr, bctr


def _is_uncond(w):
    """blr / bctr / an unconditional branch with no link."""
    return w in UNCOND or ((w >> 26) == 18 and not (w & 1))


def _branch_target(w, a):
    """Where a branch instruction goes, or None if it is not a branch."""
    op = w >> 26
    if op == 18:                                   # b / bl / ba / bla
        d = w & 0x03FFFFFC
        if d >= 0x02000000:
            d -= 0x04000000
    elif op == 16:                                 # bc and its mnemonics
        d = w & 0xFFFC
        if d >= 0x8000:
            d -= 0x10000
    else:
        return None
    return (d if (w & 2) else (a + d)) & 0xFFFFFFFF


def can_shrink(code, mask, tbytes, target, tsize):
    """May the comparison window be cut down to len(code)?

    The recorded size can be TOO LONG. `.pdata` emits one unwind row for a
    run of adjacent frameless functions, and discovery sizes what it finds as
    extent-to-next-known-start, so a row can cover several bodies --
    8215E5B0 is recorded as 156 bytes and holds six thunks, only the first of
    which anything calls.

    Shrinking is only safe with PROOF, because the same relaxation would let
    a source that produces half a function pass. All four must hold:

      1. our code ends in an UNCONDITIONAL terminator, so control cannot fall
         out of it;
      2. no branch anywhere inside our code targets the leftover range, so
         control cannot jump into it either;
      3. the retail word in that last position is an unconditional terminator
         too;
      4. every non-relocated word of the prefix agrees, and
      5. at least ONE such word exists.

    (5) is not pedantry. Without it clause (4) is vacuously true over an
    empty set: a one-instruction source whose only word is a relocated tail
    call shrinks any row that begins with a tail call, and match.py reports
    MATCH having verified nothing. Found by pointing a `b <target>` thunk at
    82697740, which printed "1 word(s) compared: 0 identical, 0 differ, 1
    differ in a relocated word" and exited 0. A check that cannot fail is
    worse than no check, and this project has a rule about that exact shape.

    (4) has to be taken UNDER THE RELOCATION MASK. Comparing raw bytes would
    never succeed for a function ending in a tail call -- the `b` displacement
    is the linker's and always differs -- so the check would silently never
    fire, which is the failure mode this project has a rule about.

    Together these say the retail function ends where ours does.
    """
    if tbytes is None or len(code) < 4 or len(code) >= tsize:
        return False
    if len(tbytes) < len(code):
        return False
    if not _is_uncond(struct.unpack_from(">I", code, len(code) - 4)[0]):
        return False                                            # (1)
    if not _is_uncond(struct.unpack_from(">I", tbytes, len(code) - 4)[0]):
        return False                                            # (3)
    lo, hi = target + len(code), target + tsize
    for i in range(len(code) // 4):
        w = struct.unpack_from(">I", code, i * 4)[0]
        t = _branch_target(w, target + i * 4)
        if t is not None and lo <= t < hi:
            return False                                        # (2)
    verified = 0
    for i in range(len(code) // 4):
        if not all(mask[i * 4:i * 4 + 4]):
            continue                                            # relocated
        if (struct.unpack_from(">I", tbytes, i * 4)[0]
                != struct.unpack_from(">I", code, i * 4)[0]):
            return False                                        # (4)
        verified += 1
    return verified >= 1                                        # (5)


def compile_one(src, flags, workdir):
    """Compile via tools/xdkcc, the one place that knows the invocation."""
    obj = Path(workdir) / (Path(src).stem + ".obj")
    blob, err = xdkcc.compile_obj(src, obj, flags, workdir)
    if blob is None:
        print("COMPILE FAILED")
        for line in (err or "").splitlines()[:6]:
            print("  cl: %s" % line)
        return None
    return obj


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    src = Path(argv[1])
    target = int(argv[2], 16)
    flags = list(DEFAULT_FLAGS)
    if "--flags" in argv:
        flags = argv[argv.index("--flags") + 1].split()
    sym_want = argv[argv.index("--sym") + 1] if "--sym" in argv else None

    img = Image()
    sizes = dict(load_inventory())
    if target not in sizes:
        print("%08X is not a known function start." % target)
        print("  The inventory holds %d function(s). If build/functions_all.txt"
              % len(sizes))
        print("  is missing or stale, run tools/inventory.py.")
        return 1
    tsize = sizes[target]
    recorded = tsize
    tbytes = img.read(target, tsize)

    work = Path("build/match")
    obj = compile_one(src, flags, work)
    if obj is None:
        return 2

    fns = coff_functions(obj.read_bytes())
    if sym_want:
        # Anchor on the mangled form: MSVC emits `?Name@@YA...`, so "?Name@@"
        # pins the whole name. A plain substring made `ClearAndHandle` also
        # select `ClearAndHandleOther`, and the tie was then broken by size --
        # which is to say, arbitrarily.
        exact = [f for f in fns if ("?" + sym_want + "@@") in f[0]]
        fns = exact or [f for f in fns if f[0] == sym_want] \
            or [f for f in fns if sym_want in f[0]]
        if len(fns) > 1:
            print("--sym %r selects %d functions; it must select one:"
                  % (sym_want, len(fns)))
            for n, c, _m in fns:
                print("    %-50s %d byte(s)" % (n, len(c)))
            return 2
    if not fns:
        print("no PowerPC function found in the object")
        return 2
    if len(fns) > 1:
        print("%d functions in the object; using the largest. "
              "Use --sym to pick one:" % len(fns))
        for n, c, _m in fns:
            print("    %-50s %d byte(s)" % (n, len(c)))
    sym, code, mask = max(fns, key=lambda f: len(f[1]))
    code, mask = trim_padding(code, mask)

    # The recorded size can be SHORT. Ghidra computes a function body from
    # reachable code, so a trailing instruction after an unconditional branch
    # -- the dead `blr` MSVC appends to a tail call -- is not counted. That
    # made sub_82807B38 read as 16 bytes when its code is 20, and a correct
    # source was reported NO MATCH.
    #
    # So when our code is longer, extend the window into the image and say so.
    # Bounded by the next known function start, and only reported as a
    # reconciliation once the extra words actually agree -- never silently.
    extended = None
    if len(code) > tsize:
        later = sorted(a for a in sizes if a > target)
        limit = (later[0] - target) if later else len(code)
        if len(code) <= limit:
            grown = img.read(target, len(code))
            if grown is not None and len(grown) == len(code):
                tail_ok = all(
                    struct.unpack_from(">I", grown, i * 4)[0]
                    == struct.unpack_from(">I", code, i * 4)[0]
                    for i in range(tsize // 4, len(code) // 4))
                if tail_ok:
                    extended = len(code)
                    tbytes, tsize = grown, len(code)

    shrunk = None
    if can_shrink(code, mask, tbytes, target, tsize):
        shrunk = (tsize, len(code))
        tbytes, tsize = tbytes[:len(code)], len(code)

    print()
    print("target  %08X  %d byte(s)" % (target, recorded))
    print("ours    %-40s %d byte(s)%s"
          % (sym[:40], len(code),
             "" if len(code) == recorded else "   <-- SIZE DIFFERS"))
    if extended:
        print()
        print("SIZE RECONCILED: the inventory records %d byte(s); our code is %d."
              % (recorded, extended))
        print("  The image's own bytes through %08X agree, so the recorded size"
              % (target + extended - 4))
        print("  is short by %d. A function ending in a tail call has an"
              % (extended - recorded))
        print("  unreachable trailing instruction that a reachability-based")
        print("  body computation does not count. Comparing %d bytes."
              % extended)
    if shrunk:
        was, now = shrunk
        print()
        print("SIZE RECONCILED THE OTHER WAY: the inventory records %d byte(s)"
              % was)
        print("  and this function is %d. Our code ends in an unconditional"
              % now)
        print("  terminator, the retail word there is one too, no branch in")
        print("  the compared range reaches %08X..%08X, and every"
              % (target + now, target + was))
        print("  non-relocated word of the prefix agrees -- so the retail")
        print("  function ends here as well and the row covers more than one")
        print("  body. One `.pdata` unwind record can span a run of adjacent")
        print("  frameless functions. Comparing %d bytes." % now)
    print()

    n = min(len(code), tsize) // 4
    same = diff = reloc_diff = 0
    for i in range(n):
        va = target + i * 4
        a = struct.unpack_from(">I", tbytes, i * 4)[0]
        b = struct.unpack_from(">I", code, i * 4)[0]
        relocated = not all(mask[i * 4 : i * 4 + 4])
        if a == b:
            same += 1
            continue
        if relocated:
            reloc_diff += 1
            flag = "r"
        else:
            diff += 1
            flag = "X"
        print(" %s %08X  want %08x  %-34s" % (flag, va, a, text(a, va)))
        print("            got  %08x  %-34s" % (b, text(b, va)))

    extra = abs(len(code) - tsize) // 4
    print()
    print("%d word(s) compared: %d identical, %d differ, %d differ in a "
          "relocated word (expected)" % (n, same, diff, reloc_diff))
    if extra:
        print("%d word(s) of length difference not compared" % extra)

    # Never report a match having compared nothing. A function every one of
    # whose words is relocated is not confirmed by this tool at all -- the
    # comparison excuses relocated words, so "0 identical, 0 differ" is an
    # empty statement dressed as a success.
    if diff == 0 and len(code) == tsize and n and same == 0:
        print("")
        print("NOT A MATCH -- all %d word(s) are relocated, so nothing was" % n)
        print("actually verified. A function whose every word is supplied by")
        print("the linker cannot be confirmed by comparison. build.py, which")
        print("RESOLVES relocations instead of excusing them, is what can")
        print("speak about it.")
        return 1
    if diff == 0 and len(code) == tsize:
        print("\nMATCH: every non-relocated word is identical.")
        return 0
    print("\nNO MATCH.")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
