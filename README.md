# ToS-Decomp

A **byte-matching C++ decompilation** of *SpongeBob's Truth or Square*
(Xbox 360, Heavy Iron Studios / THQ, 2009).

The goal is source that recompiles, through the title's original compiler, to
bytes identical to the retail image. Not a port, not a re-implementation.

> Separate from `../ToS-Port`, which is a static-recompilation PC port of the
> same game. Nothing here depends on that tree, and the two should not be
> confused. The one thing borrowed is `thirdparty/disasm/`, a copy of
> binutils' PowerPC disassembler that happened to be vendored there.

---

## Read these, in this order

| file | what it is |
|---|---|
| **README.md** | this — state, setup, how to run things |
| **SHELL-TRAPS.md** | the environment will silently corrupt your files; read before writing any script |
| **FINDINGS.md** | every established fact, with how it was measured. The long one. |
| **VMX128.md** | a verified VMX128 reference, including errata in both public sources |
| **MATCHED.md** | functions matched so far |

---

## Where the project is

```
functions known                30,630     (.pdata 21,238 + discovery 9,392)
.text covered by the inventory  99.6%     (8,432,420 of 8,467,964 bytes)
attributed as NOT the game's    8,238     (39.4% of .text BYTES)
remaining to decompile         22,392     (60.6%)
call-graph edges               85,314
vetted match candidates         4,247
FUNCTIONS MATCHED                  15
```

Matches are listed in `MATCHED.md`, all at one uniform `/O2 /Gy /GS- /fp:fast`.
Most matched on the first attempt, written straight off the disassembly by
**reading the target's register discipline instead of guessing plausible C** —
which value it keeps live, and for how long, is the specification.

The handful that resist share one thing: the compiler made a free choice the
source cannot express — instruction order, branch polarity, or register
assignment. That is diagnostic after the fact, not predictive; an attempt to
turn it into a ranking (`tools/serial.py`) was validated against every known
outcome and failed, and says so on every run.

**This is still per-function matching, not a reproducing build.** See
"What this project does not yet do" below — it is the honest limit of the
current state.

Verify the first match with:

```bash
python tools/match.py src/grid_indices.cpp 822607F0
```

That command working end to end is the project's heartbeat: it means the
image, the inventory, the XDK compiler and the comparison are all intact.

### What is attributed, and how strongly

| signal | functions | strength |
|---|---|---|
| `lib` | 6,453 | byte-for-byte match against an XDK library object — strong |
| `rtti_havok` | 1,178 | Havok vtable recovered from MSVC RTTI |
| `srcpath` | 353 | the function references a middleware source path — weak |
| `havok` | 251 | pushes a Havok profiler timer name — weak |
| `game_profiled` | 3 | pushes a `Ttz` name, so it IS the title's own |

These are the counts on the CURRENT inventory. They were stale for a while:
`attribution.txt` had been generated against the older 25,737-function
inventory and still summed to it, so `lib` read 6,332 and `rtti_havok` 673.
Derived files do not regenerate themselves — `tools/verify.py` does not yet
check for staleness, which is a known gap.

`srcpath` and `havok` are attribution by a reference a function *makes*. They
are deliberately not merged with the byte matches and have never been
spot-checked.

### The memory map

The linker grouped things, so the game's own code is in the gaps:

```
82100000..821294A0   XDK — the D3D / XTL band
82129000..822F0000   THE GAME
822F03E8..82523A1C   XDK — one 2.31 MB block
82523A1C..828A74A0   THE GAME  (engine, Havok, Scaleform, FMOD, Bink)
828A74A0..82908510   XDK — the CRT
```

---

## What this project does not yet do

Measured against how established matching decompilations are run (SM64,
Ocarina of Time, and the GC/Wii projects built on dtk/splat), this one is
strong on evidence discipline and **weak on structure**. Stated plainly so it
is not discovered later:

**1. There is a verifying build, but it is a SPLICE, not a LINK.**
`python tools/build.py` compiles every source in `src/manifest.txt`, resolves
each relocation against the retail bytes, splices the result into `.text` and
hashes the section — exit 0 only when it reproduces. That closed the worst of
this gap: relocations are now *resolved* rather than masked, so a wrong
register inside a relocated word is caught, and every resolved address is
checked to land on a real function start or in a data section.

What remains: the undecompiled code is **copied** from the original rather
than assembled from objects, so section layout, symbol ordering and
inter-object padding are still unverified. Only 436 of 8,467,964 `.text` bytes
(0.0051%) are actually built. The real link — every function as an object,
ordered by a linker script — is still ahead.

**2. The type system is started, not finished.** `include/types.h` now
carries the primitives and, more usefully, **compile-time layout assertions**:

```c
ASSERT_OFFSET(Owner, flag8C, 0x8C);
ASSERT_SIZE(Entry, 1856);          // the stride `mulli r11,r3,1856` states
```

A wrong field offset is now `error C2118: negative subscript` before anything
is compared, instead of surfacing later as a one-word diff that reads like a
scheduling problem. Every offset the 15 matched functions established is
asserted, and three negative controls confirm a corrupted one fails the build.

What is *not* done: only one type identity across files is actually supported
by evidence (`src/owner_clear.cpp`), so most structs are still per-file. That
is honest rather than lazy — see gap 3 for why merging them speculatively
would be worse than leaving them apart.

**3. TU splitting was attempted and the result is mostly NEGATIVE.**
`tools/segment.py` groups functions by adjacency and scores itself against
6,541 functions whose true object file is known by byte match. It does not
work well:

```
rule                          precision   recall
gap <= 4                        55.1%       47.0%
gap <= 64                       23.6%       62.2%
gap <= 4  AND call-related      89.1%        6.3%     <- the default
```

There is no threshold that is both accurate and complete. With `/Gy` every
function is its own COMDAT and the linker interleaves and folds them freely —
90 known objects have functions more than 4 KB apart, and 8 pairs overlap in
address order.

So segmentation is a **hint generator, not a partition**: a segment is decent
evidence that its functions share a TU, and the absence of one is no evidence
at all. That is exactly why gap 2 stops where it does — a wrong merge invents
a false type identity that compiles fine and is very hard to notice, while a
wrong split only costs duplicated effort.

The two merges that *were* made rest on direct evidence, not the clustering:
`StrLen`/`StrCopy` sit 4 bytes apart and share 12 callers;
`ClearAndHandle`/`ClearAndHandleOther` sit 4 bytes apart, read the same field
of the same argument, and write neighbouring fields.

**4. Names are invented, not recovered.** `ProcessIfReady`, `ArrayAdd`,
`GetThroughChain` are descriptions, not the title's symbols. That is normal
this early, but they should be treated as provisional. Real names exist for
part of the image (RTTI, profiler scopes, assert strings) and are not yet
applied to matched code.

**5. Progress is counted in functions, not bytes.** 15 of 30,984 says little;
byte coverage against the 60.6% that is actually the game's is the number that
matters. `tools/build.py` now reports byte coverage of `.text`, but there is
still no dashboard tracking it over time.

**6. There is no permuter.** `permute.py` scores a hand-written list of
shapes. `decomp-permuter` mutates source automatically to search register
allocation and scheduling outcomes — which is precisely the wall the four
stalled functions are stuck against.

**7. There is no CI.** Nothing re-runs the matches on commit; the regression
check is run by hand.

What *is* in good shape: the toolchain is identified and verified three
independent ways, which is the foundation everything else rests on and the
thing that most often goes wrong; relocation masking is honest and states its
denominators; library code is attributed and excluded from scope on byte
evidence; and every tool is validated against known-good answers before its
output is believed.

**In priority order the gaps are: (2) and (3) together, then (6), then
finishing (1) into a real link.**

---

## Ghidra, measured and replaced

Ghidra supplied exactly two things: the function starts `.pdata` lacks, and the
call graph. Neither needs a decompiler — a `bl` names its target in the
instruction word, so a linear sweep finds every called function and the same
sweep yields the edges. `tools/discover.py` does that, plus a data-pointer scan
for functions reachable only through a vtable.

Head to head on this image:

| | Ghidra | `discover.py` |
|---|---|---|
| functions beyond `.pdata` | 4,332 | **9,392** |
| of Ghidra's, rediscovered | — | **99.7%** (13 missed) |
| non-code wrongly listed as functions | 167 | **0** |
| call-graph edges | 73,686 | **85,315** |
| correct sizes, on the 15 whose true size the build establishes | 13/15 | **15/15** |
| wall clock | a 12 GB headless run | **1.1 s** |
| VMX128 | cannot decode it | decodes it |

The two sizes Ghidra gets wrong are both tail-call functions. It computes a
body from *reachable* code, so the unreachable `blr` MSVC appends after a tail
call is not counted and the size comes out 4 bytes short — 171 functions in
this image have that shape.

**How the extra 5,260 were checked**, because "found more" is not the same as
"found real":

* a discovered start must DECODE as an instruction. There is a 3.3 KB data
  blob at the tail of `.text` (`8291266C..8291334C`) and words inside `.text`
  that are data happen to decode as `bl`/`b` with targets landing in it. 206
  non-functions entered that way — 167 as `bl` targets, and **Ghidra lists
  the same 167**. Both sides are filtered before comparing.
* 102 of the branch-sweep starts (1.7%) fall inside a known function, and
  the biggest cluster is 15 entry points into one 84-byte host — the
  `__restgprlr` register-restore helper, whose multiple entry points are
  documented in §7e and are legitimate.
* the data-pointer scan finds 26,255 words pointing into code and **70.9%
  land exactly on an already-known function start**. `.pdata` is excluded
  from that statistic: it is the unwind table, so every word in it points at
  a function start by definition, and including it reported 83.9% — a number
  inflated by a table that is a list of the very thing being predicted.
* the data-pointer-only candidates disassemble as textbook accessors
  (`lwz r11,off(r3) / rlwinm r3,r11,... / blr`) — small vtable-only methods
  with no unwind row, which is exactly the population that should be missing.
* sizes derived as extent-to-next-start with padding trimmed agree with
  `.pdata` on **97.0%** of the 21,238 rows that can be compared.

`ghidra_scripts/` and the import path are kept for cross-checking, and
`python tools/inventory.py --ghidra` still builds the old union. Nothing in
the pipeline requires it.

---

## What you need that is not in this repo

**1. The retail disc, extracted to `game/`** — 150 files, 3.79 GiB, with
`game/DEFAULT.XEX` at its root.

**2. The Xbox 360 XDK, build 8276 (March 2009), extracted to `SDKFiles/xdk/`.**
This is the exact toolchain the game was built with, established three ways:

```
Rich header of the retail image     build 8153
PE optional header, linker version  9.0
XTL source paths in .rdata          e:\xenon\mar09\...
embedded tool banner                Microsoft (R) Xbox 360 Shader Assembler 2.0.8276.0
```

The XDK ships **two** PowerPC toolchains and only one is right:

| | version | |
|---|---|---|
| `XDK\bin\win32\cl.exe` | **15.00.8153** | correct |
| `XDK\bin\win32\link.exe` | **9.00.8153** | correct |
| `XDK\TechPreview\Mar09Compiler\` | 15.00.8327 | wrong |

**Verify any candidate copy** rather than trusting a folder name: compile one
`.cpp` and check the object's `@comp.id` symbol has `0x1FD9` (= 8153) in its
low half. `tools/objcode.py` will show it.

`link.exe` needs a one-time repair — it ships with an empty embedded manifest,
so it fails `0xC0000142` (R6034, the VC90 CRT loaded without an activation
context). Fix, using `mt.exe` from any Windows SDK:

```bash
mt -manifest link.fix.manifest "-outputresource:link.exe;#1"
```

where `link.fix.manifest` declares a dependency on
`Microsoft.VC90.CRT version 1.9.7.21022 x86`. Clear the read-only attribute
first — 7-Zip preserves it from the archive and `mt` reports "Access is denied".

**`link.exe` is not the only one.** Six EXEs ship with no manifest at all and
die the same way the first time they are run — `dumpbin`, `editbin`, `lib`,
`pgocvt`, `pgodump`, `pgomgr`. Repair them all without touching any binary:

```bash
python tools/fix_manifests.py --write
```

That writes an external `<name>.exe.manifest` beside each, the mechanism
Microsoft already used for `cl.exe` in that folder. `python tools/pemanifest.py
SDKFiles/xdk/XDK/bin/win32` reports the state of all 160 modules without
running any of them — running a broken one pops a modal dialog that blocks
until someone clicks OK.

**3. Ghidra is NO LONGER REQUIRED.** It used to supply the function starts
`.pdata` lacks, plus the call graph. `tools/discover.py` does both from the
image alone, in about a second, and measurably better — see "Ghidra, measured
and replaced" below. Keep it only if you want an independent cross-check.

---

## Rebuilding from scratch

Each step writes into `build/`, which is entirely derived and gitignored.

```bash
python tools/xex.py            # DEFAULT.XEX -> build/default.pe.exe
python tools/flatten_pe.py     # fix the section headers -> default.image.exe
python tools/verify_mapping.py # CONFIRM the mapping before trusting anything
python tools/pdata.py          # the .pdata function table -> functions.txt
```

Then Ghidra, which supplies the other 4,499 functions and the call graph:

```bash
GHIDRA_HEADLESS_MAXMEM=12G analyzeHeadless <proj> ToS \
  -import build/default.image.exe -overwrite \
  -processor "PowerPC:BE:64:A2ALT-32addr" \
  -scriptPath ghidra_scripts \
  -preScript TuneAnalysis -preScript ApplyPdata -preScript ApplyKnowledge \
  -postScript ReportAnalysis -max-cpu 12
```

then export and build the union inventory:

```bash
analyzeHeadless <proj> ToS -process default.image.exe -noanalysis \
  -scriptPath ghidra_scripts -postScript DumpFunctions build/ghidra_fn_v2.txt
python tools/inventory.py
```

Then the analysis passes, in any order:

```bash
python tools/libmatch.py --all --min-bytes 32   # XDK library byte matching (slow)
python tools/rtti.py                            # Havok classes and vtables
python tools/xeximports.py                      # 207 kernel/XAM imports, named
python tools/srcfiles.py                        # source paths referenced by code
python tools/profnames.py                       # engine profiler scope names
python tools/attribute.py                       # merge it all into one picture
python tools/candidates.py                      # pick a match target
```

The VMX128 disassembler must be built once (MSVC, x64):

```bash
cl /O2 /W0 /D_CRT_SECURE_NO_WARNINGS /I.. \
   ..\ppcdis_main.c ..\ppc-dis.c ..\disasm.c /Fe:build\ppcdis.exe
```

---

## Tools

| tool | what it does |
|---|---|
| `xex.py` | XEX2 container: parse, decrypt, decompress, verify |
| `flatten_pe.py` | rewrite section headers to match the memory image |
| `verify_mapping.py` | **decide** the VA→offset mapping against `.pdata`, both arms scored |
| `pdata.py` | walk the unwind table; picks the entry layout by marking four arms |
| `inventory.py` | `.pdata` ∪ Ghidra — the real function population |
| `peimage.py` | shared image access, `load_inventory()`, XDK region map |
| `ppcdis.py` | disassembly via binutils — **the only decoder here that knows VMX128** |
| `disasm.py` | disassemble a guest address range, annotating string references |
| `match.py` | **the matching loop**: compile a candidate, diff against the image |
| `objcode.py` | disassemble a COFF object's code |
| `libmatch.py` | match XDK library objects against the image (relocation-masked) |
| `rtti.py` | MSVC RTTI → Havok class names and vtables |
| `xeximports.py` | XEX import table + XDK import libs → 207 named thunks |
| `srcfiles.py` | source paths the code forms addresses to |
| `profnames.py` | profiler scope names around `mftb` |
| `strings.py` | classified string census |
| `xref.py` | find references to an address (`lis`+`addi`/`ori` pairs) |
| `attribute.py` | merge every signal into one scope picture |
| `candidates.py` | vetted match targets: leaf, unattributed, outside XDK, sound |
| `build.py` | **the reconstructing build**: compile, resolve relocations, hash `.text` |
| `coffreloc.py` | COFF functions with their relocation records |
| `discover.py` | function starts and the call graph, from the image alone |
| `segment.py` | probable translation units — scores itself, and mostly fails |
| `flagsweep.py` | sweep compiler flags for one source against one target |
| `permute.py` | sweep source shapes for one target at fixed flags |
| `vmx128_*.py` | four independent VMX128 validations — see `VMX128.md` |
| `rich.py` | decode a PE's Rich header — the linker's census of contributing tools |
| `rich_calibrate.py` | **measure** what each product id means, by building known flag sets |
| `compid.py` | `@comp.id` per object; `--join` cross-checks the matched library objects |
| `pemanifest.py` | which XDK tools will raise R6034 — read statically, nothing is run |
| `fix_manifests.py` | repair those, with an external manifest rather than by editing binaries |
| `verify_ghidra.py` | **superseded**; kept as a worked example of a vacuous check |

---

## Rules this project paid for

**Validate a search against something you know it must find, before believing
a negative.** A mangled regex returned zero Windows paths from an image with
64 of them. No error — just a benign-looking empty result. That is how absence
of evidence becomes evidence of absence.

**Two derivations are only a cross-check if they are independent.**
`verify_ghidra.py` compared Ghidra's memory map against our own read of the
file. Both used `PointerToRawData`. When that turned out to be the wrong
mapping, they agreed while both were wrong, and it reported "14 of 14 blocks
agree" on a program whose `.text` was entirely misplaced.

**A sound method on bad inputs gives a sound-looking answer about nothing.**
An elaborate boundary analysis decided whether 6,069 library matches landing
"inside" functions were real. With the mapping fixed, the number was 9. The
whole analysis was answering a question the bug had invented.

**Measure a repair with an instrument that can see it.** Ghidra computes a
function body at creation and never recomputes it, so measuring a disassembly
repair by body extent reports `+0` while the repair works. That produced a
false `+0` twice here.

**Never put a backslash in a heredoc.** It corrupted files four times, twice
silently. `.claude/hooks/` now blocks it; `SHELL-TRAPS.md` explains why
counting backslashes carefully is not a workable defence.

**State the denominator.** Not "24 draws" but "24 draws of 59 walked". Every
count here names its population, and a bounded search reports when its bound
was reached rather than presenting the bound as an answer.

---

## Where to pick up

**The LTCG question is settled — see §7l.** It was the top priority because it
decided whether object-level matching could work at all. It can:

```
of 1,528 objects in the retail image carrying a code-producing stamp,
1,474 (96.5%) were compiled WITHOUT /GL.  54 were not.  No C TU used /GL.
```

Measured by calibrating the Rich header's product ids against this XDK
(`tools/rich_calibrate.py`) rather than looking them up, and cross-checked
from the other direction — every one of the 610 library objects that matched
byte-for-byte into the image carries a plain C/C++/asm stamp
(`tools/compid.py --join`). The library-variant question is answered with it:
the retail build linked the **non-LTCG** variants.

So the stalled matches are not stalled by LTCG. Six functions have since
matched as compiled objects, all at addresses in the game's own bands. What
the two long-running stalls come down to is **instruction scheduling** and
**register allocation** — compiler-internal choices in functions that have
more than one legal ordering:

```
82806FD0   84 B   11/21   8 source shapes, 65 flag combinations
826C1480   76 B   13/19   branchless; 11 shapes x 72 flags, all identical
827C5198   20 B    3/5    register allocation only, 7 shapes
8215E5B0   28 B    1/7    register allocation only
```

**FIRST: keep matching.** `python tools/candidates.py` gives 2,565
vetted targets — leaf, unattributed, outside the XDK bands, at least 16 bytes,
ending in a real terminator. `tools/permute.py` scores several source shapes
against one target in a single command.

The technique that worked on the one match is in `FINDINGS.md` §7d: **read the
target's register discipline out of the disassembly instead of guessing
plausible C.** Which value it keeps live and for how long IS the
specification. Writing assignments in the target's own field order took
`826C1480` from 10/19 to 13/19.

**Larger, in rough order of value:**

1. **Havok**. 322 of 329 RTTI classes are `hk*`, so Havok is a large fraction
   of the remaining 60.6%. A matching **Havok 6.5.0-r1 for Xbox 360** would let
   `libmatch.py` eat it directly. The SDK has not turned up; RTTI identifies it
   without one, which is enough to exclude it from scope but not to link it.
2. **Scaleform GFx 3.x and FMOD Ex** for Xbox 360 — same idea.
3. **VMX128 in Ghidra.** Our tools decode it; Ghidra does not (issue #2094,
   open since 2020). Forks exist — pjsoberoi's, and 0dinD's rebase onto Ghidra
   12.0 which also fixes errors in both the SLEIGH and the documentation.
   Untried here.
4. **MSVC switch tables.** Ghidra's `PowerPCAddressAnalyzer` mishandles MSVC's
   PowerPC switch pattern. Measured over this image: 57 sites use the reported
   `lhzx` form, 347 use `lwzx`, 604 `bctr` have no switch shape at all.
5. **Spot-check the weak attributions.** 603 functions are attributed by
   `srcpath`/`havok` alone and none has been confirmed by hand.
6. **A build system and a progress dashboard.** One `.cpp` at a time does not
   scale, and objdiff-style percentage tracking is how these projects survive.

**Known open, small:**

- `vmaddcfp128` (175 sites) and `vpkd3d128` (94) have no declared XDK
  intrinsic. Decoding them is fine; whether ordinary vector expressions can
  *write* them is NOT_MEASURED.
- **What the 54 `/GL` objects are** is NOT_MEASURED — either the title's own
  translation units, or members of `xact3ltcg.lib` / `x3daudioltcg.lib`, the
  only two LTCG libraries with no matched twin. At worst 3.5% of the image.
- **Whether PGO was used** is NOT_MEASURED. `/LTCG:PGI` fails here with
  `LNK1123` on the `.pgd`, so which product id a PGO build stamps is unknown
  and the image cannot be checked for it. Not needed to explain the stalled
  matches: the branchless `826C1480` rules out branch layout by construction.

---

## Licensing

`thirdparty/disasm/` is binutils-derived (GPL). `game/` and `SDKFiles/` are
gitignored and are not distributed with this repository.
