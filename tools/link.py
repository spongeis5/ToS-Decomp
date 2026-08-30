"""Link matched functions with the title's own linker and check the result.

    python tools/link.py              link every runnable run, verify each
    python tools/link.py --list       what the runs are, and why each is or is not
    python tools/link.py 821C7C60     one run
    python tools/link.py --selftest   the negative controls (run this)

`tools/build.py` is a SPLICE: it compiles each function, resolves the
relocations itself, and writes the bytes into a copy of `.text` at the address
the manifest names. Everything between the functions is copied from the
original and never questioned. So three things it cannot see:

  * whether our functions actually PACK the way the retail ones do -- each is
    written at its own address, and nothing checks that the one before it ends
    where this one begins;
  * the PADDING between them, which no source file produces;
  * whether the ORDER is achievable at all. Our objects do not even hold the
    functions in retail order: `m_bin_free.cpp` compiles BinFree first and
    BinAlloc second, and the image has them the other way round.

This runs `link.exe` 9.00.8153 -- the retail linker, established in HANDBOOK.md
-- over a contiguous RUN of matched functions, hands it the retail order via
`/ORDER:@`, places the run AT ITS RETAIL ADDRESS, and compares the span it
produces against the image byte for byte. Padding included. Nothing excused.

Placing it takes two links and a padding COMDAT. `/BASE` must be 64K-aligned
and `.text` lands at a 64K-aligned RVA, so neither alone reaches an address
like 821C7C60: the first link measures where the linker chose to put `.text`,
and the second prepends a padding COMDAT of the remainder -- under 64K -- so
the run starts exactly where the image has it. It must be a COMDAT, because
`/ORDER` orders COMDATs and nothing else; as an ordinary section the pad was
placed AFTER all 55 ordered functions and the run began at offset zero.

WHAT A PASS MEANS, exactly. The functions in a run were each already known to
match. What is new is that the linker laid them out, end to end, at the
addresses the image gives them, and that the bytes BETWEEN them -- which no
compiler emitted and the splice never touched -- came out right too. What it
does not mean: a run is still a fragment. `--list` counts what is excluded and
why, and the 26 runs that refer outside themselves are excluded precisely
because nothing places the code they call.

THE COMPARISON IS EXACT and needs no size reconciliation, so unlike every other
tool that decides "does this match?" it does not import `can_shrink` /
`can_extend` from `match.py`. Those exist to reconcile a `.pdata` row that
covers more than one body against a single compiled function. No `.pdata` row
is consulted here: a run's extent is the sum of COMPILED lengths, first address
to last address plus last length, and the whole span is compared against the
image with no window to adjust.
"""

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import coffwrite
import matched_table
import xdkcc
from coffreloc import functions_with_relocs, COMPANION
from libmatch import trim_padding
from peimage import Image

ROOT = Path(__file__).resolve().parent.parent
LINK = ROOT / "SDKFiles/xdk/XDK/bin/win32/link.exe"
WORK = ROOT / "build/link"
MANIFEST = ROOT / "src/manifest.txt"

# Every address proven to sit in a run this tool linked, placed at its retail
# address, and found byte-identical. Written by a FULL run only. It carries a
# digest of the manifest it was measured against, because "which functions are
# linked" stops being true the moment the manifest grows, and a stale answer
# to that question would silently inflate the completeness the report claims.
LINKED = ROOT / "build/linked.txt"

# Adjacent means "no unmatched code in between". A gap of 4 is the alignment
# padding cl.exe asks for -- every function COMDAT it emits is marked
# IMAGE_SCN_ALIGN_8BYTES, so a function ending at 4 mod 8 is followed by four
# bytes of nothing. MEASURED over the manifest: of 464 consecutive pairs whose
# gap is 0..4, the gap is 4 exactly when the previous function ends at 4 mod 8,
# 464 times out of 464, and all 298 non-zero gaps are filled with zero. So a
# gap of 4 is not a hole in our knowledge, it is the linker's own arithmetic,
# and reproducing it is part of what this checks.
MAXGAP = 4

BASE = 0x82000000
FIXED_FLAGS = ["/c", "/nologo"]
DEFAULT_OPT = ["/O2", "/Gy", "/GS-", "/fp:fast"]


def compiled(rows):
    """[(addr, size, src, sym, flags)] for the manifest, sorted by address."""
    out = []
    for src, addr, sym, flags in rows:
        n = matched_table.compiled_size(src, sym, flags, addr)
        if n is not None:
            out.append((addr, n, src, sym, flags))
    out.sort()
    return out


def runs_of(sized, maxgap=MAXGAP):
    """Maximal groups of 2+ functions with no unmatched code between them."""
    if not sized:
        return []
    out, cur = [], [sized[0]]
    for row in sized[1:]:
        prev = cur[-1]
        if 0 <= row[0] - (prev[0] + prev[1]) <= maxgap:
            cur.append(row)
        else:
            out.append(cur)
            cur = [row]
    out.append(cur)
    return [r for r in out if len(r) > 1]


_OBJS = {}


def tag_for(src, flags):
    tag = Path(src).stem
    if flags:
        tag += "_" + flags.replace("/", "").replace(",", "_")
    return tag


def obj_for(src, flags):
    key = (src, flags)
    if key not in _OBJS:
        use = FIXED_FLAGS + (flags.split(",") if flags else DEFAULT_OPT)
        _OBJS[key] = xdkcc.compile_obj(ROOT / src,
                                       WORK / (tag_for(src, flags) + ".obj"),
                                       use, WORK)
    return _OBJS[key]


def pick(blob, sym):
    """The one function a manifest row names, by the same rule build.py uses."""
    fns = functions_with_relocs(blob)
    if not sym:
        return fns[0] if len(fns) == 1 else None
    for cand in ([f for f in fns if ("?" + sym + "@@") in f[0]],
                 [f for f in fns if f[0] == sym],
                 [f for f in fns if sym in f[0]]):
        if len(cand) == 1:
            return cand[0]
    return None


def undefined_externals(blob):
    """Every symbol the object declares and does not define. -> set of names."""
    if len(blob) < 20:
        return set()
    _m, _n, _t, psym, nsym, _o, _c = struct.unpack_from("<HHIIIHH", blob, 0)
    strtab = psym + nsym * 18
    out, i = set(), 0
    while i < nsym:
        o = psym + i * 18
        raw = blob[o:o + 8]
        if raw[:4] == bytes(4):
            off = struct.unpack_from("<I", blob, o + 4)[0]
            e = blob.find(bytes(1), strtab + off)
            name = blob[strtab + off:e].decode("latin1")
        else:
            name = raw.rstrip(bytes(1)).decode("latin1")
        _v, secnum, _ty, cls, naux = struct.unpack_from("<IhHBB", blob, o + 8)
        if secnum == 0 and cls == 2:
            out.add(name)
        i += 1 + naux
    return out


def describe(run):
    """-> (span, placed, needs_address, no_fixup, error).

    `placed` is [(addr, mangled name, object tag)] in retail address order.

    The two external buckets are NOT the same thing and the distinction is the
    whole reason a link is possible at all here:

      `needs_address` -- something relocates against it, so satisfying it means
      choosing an address. Refused; see `link_run`.

      `no_fixup` -- declared undefined but never named by any relocation.
      `_fltused` is the case that occurs: cl.exe emits a reference to it from
      any unit that touches floating point, purely so the linker drags in the
      CRT's FP support, and 67 of our 406 translation units do. MEASURED: of
      those 67, zero have a relocation against it. A symbol no relocation names
      cannot put a byte anywhere, so defining it changes nothing that is
      compared -- and that justification is read off each object rather than
      taken from a list of names, because a list would also excuse the next
      symbol that looked similar and did have a fixup.
    """
    first = run[0][0]
    span = run[-1][0] + run[-1][1] - first
    placed, relocated, undef = [], set(), set()
    for addr, size, src, sym, flags in run:
        blob, err = obj_for(src, flags)
        if blob is None:
            head = (err or "").splitlines()
            return span, [], set(), set(), ("%s did not compile: %s"
                                            % (Path(src).name,
                                               head[0] if head else "?"))
        got = pick(blob, sym)
        if got is None:
            return span, [], set(), set(), ("%s: no single function for %r"
                                            % (Path(src).name, sym))
        name, code, relocs = got
        code, _m = trim_padding(code, bytes([1]) * len(code))
        if len(code) != size:
            return span, [], set(), set(), (
                "%s compiled to %d bytes, the manifest run says %d"
                % (Path(src).name, len(code), size))
        placed.append((addr, name, tag_for(src, flags)))
        undef |= undefined_externals(blob)
        for r in relocs:
            if r.type in COMPANION or r.off >= len(code):
                continue
            relocated.add(r.sym)

    # One source file compiled at TWO optimisation levels inside one run
    # cannot be linked, and the reason is worth stating rather than leaving as
    # the linker's `LNK2005: already defined`. Both objects hold every function
    # in the file, so handing the linker both defines each of them twice.
    #
    # It is not a defect in this tool. It is the manifest saying that two
    # ADJACENT functions came from translation units compiled differently --
    # which is true, and which means the file holding them is not a translation
    # unit. Both cases are generated buckets (`vt_acc_11.cpp`, `vt_acc_12.cpp`)
    # that collect unrelated accessors from all over the image, so two
    # neighbours in the image really did come from two different real TUs.
    # Splitting those files by level is what unblocks it.
    levels = {}
    for addr, _size, src, _sym, flags in run:
        levels.setdefault(src, set()).add(flags)
    split = sorted(s for s, f in levels.items() if len(f) > 1)
    if split:
        return span, [], set(), set(), (
            "%s appears at %d optimisation levels in one run, so two objects "
            "would define the same symbols; it needs splitting by level"
            % (Path(split[0]).name, len(levels[split[0]])))

    have = set(n for _a, n, _t in placed)
    outside = set(e for e in undef | relocated
                  if e not in have and not e.startswith("$"))
    needs_address = set(e for e in outside if e in relocated)
    return span, placed, needs_address, outside - needs_address, None


def parse_map(text):
    """symbol -> address, from the 'Publics by Value' table of a link map."""
    out = {}
    for line in text.splitlines():
        f = line.split()
        # ' 0001:00000000  ?Name  82000400 f  obj'
        if len(f) < 3 or ":" not in f[0]:
            continue
        try:
            int(f[0].split(":")[0], 16)
            addr = int(f[2], 16)
        except ValueError:
            continue
        out[f[1]] = addr
    return out


def _do_link(tag, objs, names, base, pad, entry):
    """One invocation. -> ((pe bytes, symbol->address), None) or (None, error)."""
    extra = []
    if pad:
        (WORK / ("pad_%s.obj" % tag)).write_bytes(coffwrite.padding(pad))
        extra = ["pad_%s.obj" % tag]
        names = ["__tos_pad"] + list(names)
    (WORK / ("order_%s.txt" % tag)).write_text(
        "".join(n + "\n" for n in names))
    cmd = [str(LINK.resolve()), "/NOLOGO", "/MACHINE:PPCBE",
           "/SUBSYSTEM:XBOX", "/XEX:NO", "/FIXED", "/BASE:0x%08X" % base,
           "/NODEFAULTLIB", "/ENTRY:" + entry,
           "/ORDER:@order_%s.txt" % tag, "/OPT:NOREF", "/OPT:NOICF",
           "/INCREMENTAL:NO", "/MAP:run_%s.map" % tag, "/OUT:run_%s.exe" % tag]
    cmd += extra + objs
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=str(WORK))
    if r.returncode != 0:
        msg = [l.strip() for l in (r.stdout + r.stderr).splitlines() if l.strip()]
        return None, "link failed: " + (msg[0][:74] if msg else
                                        "exit %d" % r.returncode)
    pe = (WORK / ("run_%s.exe" % tag)).read_bytes()
    syms = parse_map((WORK / ("run_%s.map" % tag)).read_text(errors="replace"))
    return (pe, syms), None


def _text_of(pe):
    """-> (image base, .text rva, .text raw pointer) or None."""
    e = struct.unpack_from("<I", pe, 0x3C)[0]
    nsec = struct.unpack_from("<H", pe, e + 6)[0]
    osz = struct.unpack_from("<H", pe, e + 20)[0]      # SizeOfOptionalHeader
    imgbase = struct.unpack_from("<I", pe, e + 24 + 28)[0]
    for i in range(nsec):
        b = e + 24 + osz + i * 40
        if pe[b:b + 8].rstrip(bytes(1)) == b".text":
            va, _rawsz, rawptr = struct.unpack_from("<III", pe, b + 12)
            return imgbase, va, rawptr
    return None


def link_run(run, order=None, shift=0, pad_delta=0):
    """-> (status, span, detail). True identical / False differs / None untested.

    Externals that something relocates against are refused rather than stubbed.
    The obvious stub -- an ABSOLUTE COFF symbol at the address read out of the
    image -- does not work, and that is MEASURED, not assumed: `link.exe`
    9.00.8153 answers a REL24 against an ABSOLUTE symbol with `LNK2013: fixup
    overflow` at every value tried, twelve of twelve, including values pointing
    at the linked code itself. It objects to the symbol kind, not the distance.
    The other route, `/FORCE:UNRESOLVED`, would let the linker invent the
    address -- the one thing that must not happen, because the bytes would then
    agree with a number the linker chose rather than one the image states.

    `order`, `shift` and `pad_delta` exist for `--selftest` and are not used
    otherwise: `order` replaces the symbol order handed to `/ORDER:@`, `shift`
    moves the address the linked bytes are compared against, and `pad_delta`
    moves where the run is placed. All three are ways of deliberately getting
    it wrong, so a control can require this to notice.
    """
    span, placed, needs_address, no_fixup, err = describe(run)
    first = run[0][0]
    if err:
        return None, span, err
    if needs_address:
        return None, span, ("needs an address for %d symbol(s): %s"
                            % (len(needs_address),
                               ", ".join(sorted(needs_address))[:56]))

    if first % 8:
        return None, span, ("the run starts at %08X, which is not 8-aligned; "
                            "cl.exe marks every function COMDAT "
                            "ALIGN_8BYTES, so the linker cannot place it there"
                            % first)

    tag = "%08X" % first
    names = [n for _a, n, _t in placed] if order is None else list(order)
    (WORK / ("stubs_%s.obj" % tag)).write_bytes(
        coffwrite.absolutes(dict((s, 0) for s in sorted(no_fixup))))
    objs = ["stubs_%s.obj" % tag]
    for _a, _n, t in placed:
        if t + ".obj" not in objs:
            objs.append(t + ".obj")

    # TWO LINKS. The first only measures where the linker chose to put .text;
    # the second is the real one. `/BASE` must be 64K-aligned and .text sits at
    # a 64K-aligned RVA, so neither alone can land a run on an address like
    # 821C7C60 -- the remainder is made up by a padding COMDAT ordered ahead of
    # everything, which is under 64K and is measured rather than assumed.
    got, err = _do_link(tag + "_probe", objs, names, BASE, 0, placed[0][1])
    if got is None:
        return None, span, err
    t = _text_of(got[0])
    if t is None:
        return None, span, "the linked image has no .text"
    _ib, textrva, _ptr = t

    base = (first - textrva) & ~0xFFFF
    pad = first - (base + textrva) + pad_delta
    if pad < 0 or pad % 4:
        # Reachable only from --selftest's pad_delta; a real run is 8-aligned
        # and so is its padding. Reported rather than raised, so a control
        # that asks for something impossible reads as untestable instead of
        # as a crash in the tool.
        return None, span, ("cannot place %08X: .text sits at rva %#x and the "
                            "arithmetic leaves %d" % (first, textrva, pad))
    got, err = _do_link(tag, objs, names, base, pad, placed[0][1])
    if got is None:
        return None, span, err
    pe, syms = got
    t = _text_of(pe)
    if t is None:
        return None, span, "the linked image has no .text"
    imgbase, va, rawptr = t

    for _a, name, _t in placed:
        if name not in syms:
            return None, span, "the map does not locate %s" % name[:40]

    # THE PLACEMENT IS PART OF THE RESULT, not a means to it. Every function
    # must land on its own retail address -- not merely at the right offset
    # from its neighbours -- so a run that comes out right at the wrong place
    # is a difference, and says so.
    for addr, name, _t in placed:
        if syms[name] != addr:
            return False, span, (
                "%s was linked at %08X, the image has it at %08X"
                % (name[:36], syms[name], addr))

    off = rawptr + (first - imgbase) - va
    if off < 0 or off + span > len(pe):
        return None, span, ("the run lands at %08X, outside the linked file"
                            % first)
    got_bytes = pe[off:off + span]
    want = Image().read(first + shift, span)
    if want is None or len(want) != span:
        return None, span, ("could not read %d image bytes at %08X"
                            % (span, first + shift))
    if got_bytes == want:
        return True, span, ("%d function(s), %d file(s), %d B pad%s"
                            % (len(placed), len(objs) - 1, pad,
                               "" if not no_fixup else
                               ", %d no-fixup extern" % len(no_fixup)))

    diffs = [i for i in range(0, span, 4)
             if got_bytes[i:i + 4] != want[i:i + 4]]
    return False, span, ("%d of %d word(s) differ, first at +%#x: image %s, "
                         "linked %s"
                         % (len(diffs), span // 4, diffs[0],
                            want[diffs[0]:diffs[0] + 4].hex(),
                            got_bytes[diffs[0]:diffs[0] + 4].hex()))


# --------------------------------------------------- what counts as COMPLETE

def _manifest_digest():
    import hashlib
    return hashlib.sha256(MANIFEST.read_bytes()).hexdigest()[:16]


def write_linked(ok_addrs):
    """Record every address that a full run proved linked, placed and equal."""
    lines = ["# Addresses inside a run tools/link.py linked, PLACED at its",
             "# retail address, and found byte-identical. Written only by a",
             "# full run. Consumed by tools/report.py to decide which units",
             "# are complete -- so it carries the digest of the manifest it",
             "# was measured against, and a reader must refuse it if that",
             "# has moved.",
             "manifest %s" % _manifest_digest(),
             "count %d" % len(ok_addrs)]
    lines += ["%08X" % a for a in sorted(ok_addrs)]
    LINKED.write_text("\n".join(lines) + "\n")


def linked_addresses():
    """-> (set of addresses, None) or (None, why it cannot be answered).

    NEVER returns an empty set to mean "not measured". Nothing linked and
    never asked are different states, and collapsing them reports 0% complete
    for a project that simply has not run the link -- absence of evidence
    rendered as evidence of absence, which is the failure this repository
    exists to avoid.
    """
    if not LINKED.exists():
        return None, ("%s has not been written; run `python tools/link.py`"
                      % LINKED.relative_to(ROOT).as_posix())
    want = _manifest_digest()
    got, out = None, set()
    for line in LINKED.read_text().splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        if line.startswith("manifest "):
            got = line.split()[1]
            continue
        if line.startswith("count "):
            continue
        try:
            out.add(int(line, 16))
        except ValueError:
            pass
    if got != want:
        return None, ("%s was measured against a different src/manifest.txt "
                      "(%s, now %s); re-run `python tools/link.py`"
                      % (LINKED.relative_to(ROOT).as_posix(), got, want))
    return out, None


def complete_addresses(linked=None):
    """Every matched function that is COMPLETE, by address. -> (set, err)

    Same two clauses as `complete_sources`, at the granularity objdiff.json
    uses: one unit per function. A function qualifies when it is inside a run
    this tool linked and placed, and its source file defines nothing the
    manifest does not name.

    THIS IS THE ONE THAT REACHES decomp.dev. `report.bin` is generated by
    objdiff-cli from `objdiff.json`, not by `tools/report.py`, and objdiff-cli
    sums `complete_code` over the units whose metadata says complete. That
    flag used to be set to "this function's bytes match", so the published
    report claimed 34,096 bytes COMPLETE -- i.e. linked -- at a time when
    nothing whatever was linked. Matched is not complete; that is the whole
    reason the schema carries both.
    """
    if linked is None:
        linked, err = linked_addresses()
        if err:
            return None, err

    by_unit = {}
    for src, addr, sym, flags in matched_table.rows():
        by_unit.setdefault((src, flags), []).append(addr)

    out = set()
    for (src, flags), addrs in by_unit.items():
        blob, _cerr = obj_for(src, flags)
        if blob is None:
            continue
        if len(functions_with_relocs(blob)) != len(addrs):
            continue                      # the object holds unchecked code
        for a in addrs:
            if a in linked:
                out.add(a)
    return out, None


def complete_sources(linked=None):
    """Which source files are COMPLETE. -> (set of src, [(src, why not)], err)

    A unit is complete when

      (a) its object defines no function the manifest does not name, and
      (b) every one of those functions lies in a run this tool linked, placed
          at its retail address, and found byte-identical.

    (a) is not a formality. 61 of 406 units currently fail it: the file
    defines a helper that was written to shape the caller's codegen and is
    not itself matched against anything. `build.py` splices only the named
    function so it never notices, but a LINK takes the whole object, and
    those bytes would occupy real addresses having been compared to nothing.

    (b) is the difference between matched and linked, which is what
    `complete` means in objdiff's schema and what decomp.dev renders as a
    separate figure from the matched percentage.
    """
    if linked is None:
        ok, err = linked_addresses()
        if err:
            return None, None, err
    else:
        ok = linked

    by_unit = {}
    for src, addr, sym, flags in matched_table.rows():
        by_unit.setdefault((src, flags), []).append(addr)

    complete, why = set(), []
    for (src, flags), addrs in by_unit.items():
        blob, cerr = obj_for(src, flags)
        if blob is None:
            why.append((src, "did not compile"))
            continue
        n = len(functions_with_relocs(blob))
        if n != len(addrs):
            why.append((src, "the object holds %d function(s), the manifest "
                             "names %d" % (n, len(addrs))))
            continue
        missing = [a for a in addrs if a not in ok]
        if missing:
            why.append((src, "%d of %d function(s) are not in a linked run"
                             % (len(missing), len(addrs))))
            continue
        complete.add(src)

    # A source appearing at two flag levels is complete only if BOTH are.
    for src, _reason in why:
        complete.discard(src)
    return complete, why, None


# ---------------------------------------------------------------- self-test

def selftest():
    """Require this check to FAIL on things it must not accept.

    A link that reports IDENTICAL is worth nothing until the same code path
    reports a difference when handed one. Each control breaks exactly one fact
    and requires a non-identical answer:

      1. the run linked normally                        must be IDENTICAL
      2. the same run with the ORDER REVERSED           must NOT be
      3. the same run compared 4 bytes off              must NOT be
      4. the same run with the first two functions
         swapped in the order file                      must NOT be
      5. the same run placed 8 bytes late               must NOT be
      6. the same run placed 8 bytes early               must NOT be

    Control 2 is the one that matters most: without it, a pass would only mean
    the linker happened to emit the objects in an order that agreed, and
    `/ORDER` might not be doing anything at all. Controls 5 and 6 are the same
    argument for the placement: a run laid out correctly at the wrong address
    is not a reproduction of anything.
    """
    sized = compiled(matched_table.rows())
    allruns = runs_of(sized)
    # A run whose functions are NOT all identical bytes, or reversing it would
    # legitimately produce the same span and control 2 would be vacuous.
    img = Image()
    pick_run = None
    for run in sorted(allruns, key=lambda r: -(r[-1][0] + r[-1][1] - r[0][0])):
        _s, placed, needs, _nf, err = describe(run)
        if err or needs or len(placed) < 2:
            continue
        bodies = [img.read(a, n) for a, n, _s2, _y, _f in run]
        if len(set(bodies)) == len(bodies) and len(set(
                len(b) for b in bodies)) > 1:
            pick_run = run
            break
    if pick_run is None:
        print("SELFTEST CANNOT RUN: no linkable run with distinct function")
        print("bodies of differing length was found, so a reversed-order")
        print("control would be vacuous. This is a failure, not a pass.")
        return 1

    first = pick_run[0][0]
    span0 = pick_run[-1][0] + pick_run[-1][1] - first
    names = [n for _a, n, _t in describe(pick_run)[1]]
    print("controls run against %08X..%08X, %d bytes, %d functions\n"
          % (first, first + span0, span0, len(names)))

    swapped = list(names)
    swapped[0], swapped[1] = swapped[1], swapped[0]
    controls = [
        ("the run, linked as normal", dict(), True),
        ("order file reversed", dict(order=list(reversed(names))), False),
        ("compared 4 bytes off", dict(shift=4), False),
        ("first two functions swapped", dict(order=swapped), False),
        ("placed 8 bytes late", dict(pad_delta=8), False),
        ("placed 8 bytes early", dict(pad_delta=-8), False),
    ]

    # A corruption control must come back FALSE -- looked, and saw a
    # difference. `None` means the tool could not test at all, and accepting
    # that as a pass is how a control stops testing anything: the reversed
    # order originally moved the anchor out of the file, the answer was "cannot
    # read", and requiring merely "not identical" called that a success.
    bad = 0
    for what, kwargs, want in controls:
        st, _span, detail = link_run(pick_run, **kwargs)
        passed = (st is want)
        if not passed:
            bad += 1
        print("  %-30s %-7s expected %-11s got %-9s %s"
              % (what, "ok" if passed else "FAILED",
                 "IDENTICAL" if want else "a difference",
                 {True: "IDENTICAL", False: "differs",
                  None: "UNTESTABLE"}[st], detail[:44]))

    # COMPLETENESS is a second claim and needs its own controls. It is the
    # one that reaches decomp.dev as `complete_code`, and it was WRONG in the
    # flattering direction for as long as it existed: objdiff.json set
    # `complete` to "this function's bytes match", so the published report
    # said 34,096 bytes were complete -- linked -- while build.py printed
    # "this is a SPLICE, not yet a LINK" on every single run.
    #
    # Each control feeds complete_sources an explicit `linked` set, so none of
    # them depends on build/linked.txt existing or being fresh.
    print("")
    here = set(a for a, _n, _s, _y, _f in pick_run)
    allm = set(a for _s, a, _y, _f in matched_table.rows())

    def n_complete(linked):
        comp, _why, err = complete_sources(linked=linked)
        return None if err else len(comp)

    base = n_complete(here)
    none_linked = n_complete(set())
    dropped = n_complete(here - {sorted(here)[0]})
    everything = n_complete(allm)

    cc = [
        ("nothing linked -> nothing complete", none_linked == 0,
         "%s complete" % none_linked),
        ("this run linked -> some complete", (base or 0) > 0,
         "%d complete" % (base or 0)),
        ("drop one address -> fewer complete", (dropped is not None
                                                and base is not None
                                                and dropped < base),
         "%s complete, was %s" % (dropped, base)),
        # Clause (a) has to bite on its own. With EVERY matched address
        # declared linked, the only thing that can still hold a unit back is
        # its object defining a function the manifest never named.
        ("all linked -> units still held back by "
         "unnamed functions in the object",
         everything is not None and everything < len(
             set(s for s, _a, _y, _f in matched_table.rows())),
         "%s of %d unit(s)" % (everything,
                               len(set(s for s, _a, _y, _f
                                       in matched_table.rows())))),
    ]
    for what, passed, detail in cc:
        if not passed:
            bad += 1
        print("  %-58s %-7s %s"
              % (what, "ok" if passed else "FAILED", detail))
    controls = controls + cc
    print("")
    if bad:
        print("%d of %d control(s) FAILED. A control that stops failing means"
              % (bad, len(controls)))
        print("this check reports success without being able to see the")
        print("failure it exists to catch.")
        return 1
    print("%d of %d controls pass: the check reports IDENTICAL for the run,"
          % (len(controls), len(controls)))
    print("and looked and saw a difference for every way of getting it wrong")
    print("that was tried.")
    return 0


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    if not LINK.exists():
        print("%s is missing -- see HANDBOOK.md, 'What you need that is not"
              % LINK)
        print("in this repo'. Nothing was linked.")
        return 1
    WORK.mkdir(parents=True, exist_ok=True)

    if "--selftest" in argv:
        return selftest()

    if "--units" in argv:
        comp, why, err = complete_sources()
        if err:
            print("cannot say which units are complete: %s" % err)
            return 1
        byreason = {}
        for src, reason in why:
            key = reason.split("(")[0].strip() if "(" in reason else reason
            byreason.setdefault(key[:56], []).append(src)
        print("%d source unit(s) are COMPLETE: every function they define is"
              % len(comp))
        print("matched, and every one is in a run link.py placed at its retail")
        print("address and found byte-identical.\n")
        for src in sorted(comp):
            print("  %s" % src)
        print("")
        print("NOT complete, %d unit(s), by reason:" % len(set(s for s, _w in why)))
        for key in sorted(byreason, key=lambda k: -len(byreason[k])):
            print("  %4d  %s" % (len(byreason[key]), key))
            for s in sorted(byreason[key])[:3]:
                print("          %s" % s)
            if len(byreason[key]) > 3:
                print("          ... and %d more" % (len(byreason[key]) - 3))
        return 0

    sized = compiled(matched_table.rows())
    allruns = runs_of(sized)
    if args:
        want = set(int(a, 16) for a in args)
        allruns = [r for r in allruns if r[0][0] in want]
        if not allruns:
            print("no run of 2+ adjacent matched functions starts at %s"
                  % ", ".join(args))
            return 1

    if "--list" in argv:
        print("%d run(s) of 2+ adjacent matched functions, of %d manifest rows"
              % (len(allruns), len(sized)))
        print("")
        print("%-19s %6s %4s %s" % ("span", "bytes", "fn", "status"))
        nlink = nblocked = blocked_bytes = 0
        for run in sorted(allruns,
                          key=lambda r: -(r[-1][0] + r[-1][1] - r[0][0])):
            span, placed, needs, nofix, err = describe(run)
            if err:
                status = err
            elif needs:
                status = "needs an address for %d symbol(s)" % len(needs)
            else:
                status = "linkable"
                if nofix:
                    status += " (%d no-fixup extern)" % len(nofix)
            if err or needs:
                nblocked += 1
                blocked_bytes += span
            else:
                nlink += 1
            print("%08X..%08X %6d %4d %s"
                  % (run[0][0], run[0][0] + span, span, len(run), status))
        print("")
        print("%d linkable, %d blocked (%d of %d run bytes blocked)"
              % (nlink, nblocked, blocked_bytes,
                 sum(r[-1][0] + r[-1][1] - r[0][0] for r in allruns)))
        return 0

    ok = bad = 0
    okbytes = skipbytes = 0
    skips = []
    ok_addrs = set()
    print("Linking with %s\n" % LINK.name)
    for run in sorted(allruns, key=lambda r: r[0][0]):
        first = run[0][0]
        status, span, detail = link_run(run)
        if status is True:
            for a, _n, _s, _y, _f in run:
                ok_addrs.add(a)
        if status is None:
            # NEVER silently. An earlier version printed a skipped run only
            # when one was asked for by address, so a full pass reported 59
            # links out of 152 runs and said nothing about the other 93 -- of
            # which 67 were ones `--list` had just called linkable. The count
            # was honest and the silence made it read as "the rest are
            # blocked", which is how a tool that failed reports a benign value.
            skips.append((first, span, detail))
            skipbytes += span
            continue
        if status:
            ok += 1
            okbytes += span
            print("  %08X..%08X  %5d B  LINKED AND IDENTICAL   %s"
                  % (first, first + span, span, detail))
        else:
            bad += 1
            print("  %08X..%08X  %5d B  DIFFERS  %s"
                  % (first, first + span, span, detail))

    if skips:
        print("")
        print("NOT LINKED -- %d run(s), %d byte(s), grouped by reason:"
              % (len(skips), skipbytes))
        why = {}
        for first, span, detail in skips:
            key = detail if detail.startswith("link failed") else \
                detail.split(":")[0]
            why.setdefault(key[:70], []).append((first, span))
        for key in sorted(why, key=lambda k: -len(why[k])):
            got = why[key]
            print("  %4d run(s), %6d B  %s"
                  % (len(got), sum(s for _a, s in got), key))
            for first, span in got[:3]:
                print("        %08X  %d B" % (first, span))
            if len(got) > 3:
                print("        ... and %d more" % (len(got) - 3))

    total = sum(r[-1][0] + r[-1][1] - r[0][0] for r in allruns)
    print("")
    print("%d run(s) linked and identical, %d differ, %d not linked"
          % (ok, bad, len(skips)))
    print("%d byte(s) of .text produced by link.exe and equal to the image at"
          % okbytes)
    print("their retail addresses, ordering and padding included -- %d of %d"
          % (ok, len(allruns)))
    print("run(s), %d of the %d byte(s) those runs span. The rest is listed"
          % (okbytes, total))
    print("above, not omitted.")

    # Only a FULL run may write the record. A run restricted to one address
    # knows nothing about the others, and letting it write would shrink the
    # set to whatever was asked for last -- which report.py would then read as
    # "the rest stopped being linked".
    if not args:
        write_linked(ok_addrs)
        print("")
        print("wrote %s: %d address(es) in a linked, placed, identical run"
              % (LINKED.relative_to(ROOT).as_posix(), len(ok_addrs)))
        comp, why, err = complete_sources()
        if err:
            print("  complete units: %s" % err)
        else:
            print("  %d of %d source unit(s) are COMPLETE -- every function"
                  % (len(comp), len(comp) + len(set(s for s, _w in why))))
            print("  they define is matched AND in a linked run")
    if bad:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
