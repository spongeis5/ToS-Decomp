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
vetted match candidates         4,231
FUNCTIONS MATCHED                 145     (6,248 bytes of .text built)
```

Matches are listed in `MATCHED.md`, whose table is generated from
`src/manifest.txt` rather than maintained by hand. **The retail build did not
use one optimisation level everywhere** — of the 145, 64 are `/O2` only, 22
are `/O2 /Os` only and 59 compile identically either way. The level is a
property of the translation unit: `python tools/flagpairs.py` compiles every
match at both levels and finds **25 informative adjacent pairs and 25
agreements, no disagreement** — the insensitive half is excluded, because
counting it would report near-total agreement whatever the truth was.
`src/manifest.txt` records the level per unit.
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
inter-object padding are still unverified. Only 6,248 of 8,467,964 `.text` bytes
(0.074%) are actually built. The real link — every function as an object,
ordered by a linker script — is still ahead.

**2. The type system is started, not finished.** `include/types.h` now
carries the primitives and, more usefully, **compile-time layout assertions**:

```c
ASSERT_OFFSET(Owner, flag8C, 0x8C);
ASSERT_SIZE(Entry, 1856);          // the stride `mulli r11,r3,1856` states
```

A wrong field offset is now `error C2118: negative subscript` before anything
is compared, instead of surfacing later as a one-word diff that reads like a
scheduling problem. Every offset the matched functions establish is
asserted, and two negative controls confirm a corrupted one fails the build.

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

The merges that *were* made rest on direct evidence, not the clustering. The
largest is the string routines: `tools/flagpairs.py` shows StrLen, StrCopy,
StrCopyN, StrCompareN, StrCompareI and StrCompareNI as six consecutive
`/O2`-only functions spanning 82540728..82540968 with four agreeing adjacent
pairs, which is a translation unit identified by measurement rather than by
guess. The two older ones:
`StrLen`/`StrCopy` sit 4 bytes apart and share 12 callers;
`ClearAndHandle`/`ClearAndHandleOther` sit 4 bytes apart, read the same field
of the same argument, and write neighbouring fields.

**4. Names are invented, not recovered.** `ProcessIfReady`, `ArrayAdd`,
`GetThroughChain` are descriptions, not the title's symbols. That is normal
this early, but they should be treated as provisional. Real names exist for
part of the image (RTTI, profiler scopes, assert strings) and are not yet
applied to matched code.

**5. Progress is counted in functions, not bytes.** 64 of 30,630 says little;
byte coverage against the 60.6% that is actually the game's is the number that
matters. `tools/build.py` now reports byte coverage of `.text`, but there is
still no dashboard tracking it over time.

**6. The permuter exists but does not reach the wall.** `tools/permuter.py`
mutates source automatically and validates against a known answer
(`--selftest` rediscovers the member-function match for `sub_826C0FC8`). It
has not cracked a single stall: six of them are one shape — a chained load
where the target reuses `r11` and we allocate fresh registers — and none of
its seven mutations touches register allocation.

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

Everything under `build/` is derived and gitignored, so a fresh clone has the
sources and none of the data. This is the whole sequence, in order. It takes
a couple of minutes, all of it except `libmatch` in seconds.

**1. The VMX128 disassembler, once.** Several tools degrade loudly without it
and one (`discover.py`) skips a filter rather than apply a wrong one, so build
it first. MSVC, x64, from `thirdparty/disasm/`:

```bash
cl /O2 /W0 /D_CRT_SECURE_NO_WARNINGS /I.. ..\ppcdis_main.c ..\ppc-dis.c ..\disasm.c /Fe:build\ppcdis.exe
```

**2. Unpack the image and establish the mapping.** Run `verify_mapping.py`
before trusting anything downstream: it scores both candidate VA→offset
mappings against `.pdata` and the wrong one still produces plausible-looking
instructions.

```bash
python tools/xex.py             # game/DEFAULT.XEX -> build/default.pe.exe
python tools/flatten_pe.py      # section headers -> build/default.image.exe
python tools/verify_mapping.py  # DECIDE the mapping, both arms scored
python tools/pdata.py           # the unwind table -> build/functions.txt
```

**3. Find the functions and the call graph.** About a second, and no Ghidra —
see "Ghidra, measured and replaced" for why.

```bash
python tools/discover.py        # branch sweep + data pointers + decodability
python tools/inventory.py       # .pdata UNION discovery -> functions_all.txt
```

`tools/addrtaken.py` is a third source — function pointers formed in code —
and finds 1,252 starts the other two cannot see. It needs
`build/switch_targets.txt` first (see step 5) and is **not merged into the
inventory**; see §7r for why, and for the calibration that says its output is
real.

**4. The analysis passes.** `libmatch` is the slow one, a few minutes.

```bash
python tools/libmatch.py --all --min-bytes 32   # XDK library byte matching
python tools/rtti.py                            # Havok classes and vtables
python tools/xeximports.py                      # 207 kernel/XAM imports, named
python tools/srcfiles.py                        # source paths the code forms
python tools/profnames.py                       # profiler scope names
python tools/attribute.py                       # merge every signal
python tools/candidates.py                      # vetted match targets
```

**Order matters here.** `candidates.py` reads `attribution.txt` and SKIPS any
inventory entry missing from it, so running it against a stale attribution
silently hides thousands of candidates — that happened, and the list read
2,542 instead of 4,231.

**5. Optional, none of it on the critical path:**

```bash
python tools/switches.py        # decode switch dispatch; checked by verify.py
python tools/segment.py         # probable translation units (weak — see its
                                #   --validate, it mostly fails)
python tools/objdiff_export.py  # ELF pairs + objdiff.json for visual diffing
python tools/dumptext.py        # full .text disassembly, 83 MB, only needed
                                #   by vmx128_intrinsics.py
```

**6. Confirm the whole thing.**

```bash
python tools/verify.py
```

### If you are setting up the XDK for the first time

`link.exe` and six other EXEs ship without a VC90 activation context and die
with R6034 the first time they run. Repair every one without touching a
binary:

```bash
python tools/fix_manifests.py --write
```

`python tools/pemanifest.py SDKFiles/xdk/XDK/bin/win32` reports the state of
all 160 modules without running any of them, which matters because running a
broken one pops a modal dialog that blocks until someone clicks OK.

---

## Tools

| tool | what it does |
|---|---|
| `xex.py` | XEX2 container: parse, decrypt, decompress, verify |
| `flatten_pe.py` | rewrite section headers to match the memory image |
| `verify_mapping.py` | **decide** the VA→offset mapping against `.pdata`, both arms scored |
| `pdata.py` | walk the unwind table; picks the entry layout by marking four arms |
| `inventory.py` | `.pdata` ∪ discovery — the real function population; `--addrtaken` folds in the third source |
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
| `addrtaken.py` | a THIRD discovery source: function pointers formed in code by `lis`+`addi` |
| `batch.py` | dump the next N candidates with disassembly, ranked by CALLER COUNT |
| `matched_table.py` | regenerate MATCHED.md's table from the manifest; `--check` catches drift |
| `flagpairs.py` | compile every match at BOTH levels and score the adjacency claim |
| `test_shrink.py` | six cases on `match.can_shrink`, five of which must refuse |
| `segment.py` | probable translation units — scores itself, and mostly fails |
| `permuter.py` | automatic source mutation; `--selftest` rediscovers a known match |
| `objdiff_export.py` | synthesize ELF pairs + `objdiff.json` for visual diffing |
| `dumptext.py` | full `.text` disassembly to a file (VMX128-aware) |
| `switches.py` | decode MSVC switch dispatch; check nothing listed as a function is a case body |
| `verify.py` | **run everything**, including five negative controls |
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

**A plausible sentence is not a measurement.** `sub_827007E8` matched under
`/Os` and the idea was dropped because "its two nearest neighbours are the
identical idiom and use the other register". They are 8.5 KB away. That is
not a neighbourhood, it was never evidence, and the real test -- do ADJACENT
functions agree? -- took one afternoon and produced six pairs out of six.
Eight matches were sitting behind that sentence.

**A check that cannot fail is worse than no check.** The `.text` hash was
computed over a splice that only ever contained bytes already proven equal,
so `rebuilt == original` was true by construction. It printed "REPRODUCES
BYTE FOR BYTE" for a test with no power -- the same shape as the
`verify_ghidra.py` mistake already recorded here. `tools/verify.py` now runs
five negative controls, each corrupting one fact and requiring the build to
fail.

**A relaxation of a check needs its own negative controls, written at the
same time.** `match.py` was taught to shrink its comparison window when the
recorded size covers more than one function -- a real defect, and the fix
unlocked four matches. The proof had four clauses and clause four was "every
non-relocated word of the prefix agrees", which is **vacuously true over an
empty set**: a one-instruction source whose only word is a relocated tail
call shrank any row starting with a tail call and printed MATCH having
verified nothing. That hole existed for about an hour and was found by
someone else pointing a thunk at 82697740. `tools/test_shrink.py` now has
seven cases of which five must REFUSE, and `match.py` additionally refuses to
report a match when every compared word was relocated.

**A `finally` does not run when the process is killed.** `verify.py`'s
negative controls corrupt a real source file, run the build, and restore it
in a `finally`. A two-minute command timeout killed one mid-control and left
`src/manifest.txt` holding a corrupted address; the next run reported four
failures, one of them reading "pattern absent, test is invalid", which is a
confusing way to be told the tree is dirty. The original text now goes to
disk BEFORE the corruption and is restored at the next startup if it is still
there.

**Two tools that measure the same thing must be made to agree, or the
disagreement must be explained.** `tools/objdiff_export.py` reported 138
units complete while `tools/verify.py` reported 145 matches, and the gap was
exactly the seven functions whose `.pdata` row covers more than one body:
the exporter read the target at the recorded size and had none of the
reconciliation `match.py` and `build.py` had grown. Seven verified matches
were being shown as failures in the visual diff, which is the direction that
gets ignored rather than investigated. Both now agree at 145.

**State the denominator.** Not "24 draws" but "24 draws of 59 walked". Every
count here names its population, and a bounded search reports when its bound
was reached rather than presenting the bound as an answer.

---

## Where to pick up

**Run this first. If it passes, everything below is true.**

```bash
python tools/verify.py
```

12 checks: five tools, 64 of 64 matches, and five negative controls that each
corrupt one fact and require the build to fail. A failing negative control is
the serious kind -- it means a check reports success without being able to
detect the failure it exists to detect.

### The fastest way to add matches

It is a batch process, and it runs at roughly 70% first-attempt success. In
one session it took the count from 64 to 116.

```bash
python tools/batch.py 40 --no-vmx        # the next 40, ranked by CALLERS
```

Ranking by caller count rather than by size or address is the whole point: a
function with 40 callers is a shared accessor whose shape recurs, so
recognising it once pays repeatedly. `batch.py` excludes anything already in
`src/manifest.txt` or `src/attempts.txt`, resolves `lis`/`addi` references
and annotates strings, so a candidate that looks unreadable usually is not.

Then: write sources for the twenty that read clearly, compile all twenty at
once, put the matches in `src/manifest.txt` and the near-misses in
`src/attempts.txt`. Do not guess plausible C -- **read the register
discipline off the listing.** Which value the function keeps live, and for
how long, IS the specification.

**This parallelises well.** Four agents each given eight candidates, the
`MATCHED.md` levers and the rule that only `tools/match.py` printing MATCH
counts, returned 6/8, 6/8, 8/8 and 8/8. Three things make it work:

* **a distinct filename prefix each** — they share `src/` AND the scratchpad,
  and two agents did overwrite each other's probe files of the same name;
* **integrate centrally** — re-run `match.py` yourself on every claim before
  it goes in the manifest, and run `build.py` after, which is stricter: it
  RESOLVES relocations rather than excusing them, and it caught an unhandled
  TLS relocation that `match.py` had passed;
* **verify serially.** `tools/verify.py` compiles the whole manifest and runs
  the build six more times; under load it exceeds a two-minute timeout, and
  one build failed transiently while four agents were compiling. Run it in
  the background, after they finish, and read the log.

`MATCHED.md` carries the idiom table and, more valuable, the **levers**: the
`do/while` that MSVC never rotates, naming or un-naming a local to move a
load, `||` versus a sequence of `if`s, `lwzx` operand order, the
`__declspec(thread)` tell. Each was the whole difference on some function.

### The flag column

The retail build did NOT use one optimisation level everywhere. Eight of the
64 need `/O2 /Os`; the rest need `/O2`. Adjacent functions always agree and
distant ones need not, so the level is a per-translation-unit property --
`src/manifest.txt` records it per unit with `flags=`. See §7m; this was
claimed the other way round for a long time and the claim was wrong.

**If a function has the right instructions but the wrong registers, try the
other level before anything else.** That was worth eight matches.

### Looking around visually

```bash
python tools/objdiff_export.py     # then open the folder in objdiff
```

Verified against objdiff-cli 3.8.0. It will not decode VMX128, so the
engine's vector maths will not render; `tools/disasm.py` is the reader that
knows it.

### What still resists

Fifteen near-misses, all in `src/attempts.txt` and all visible in objdiff.
Each has the right instructions and differs only in a decision made inside
the compiler. The score is words identical of words compared.

```
82806FD0    chunked_at      branch polarity -- bgtlr vs ble-, a probability call
826C1480    init12          instruction order -- one store among five loads
8215E5B0    arg_shuffle     register assignment across an argument permutation
82600AD0    list_insert     a reloaded field the compiler will not keep in a register
82639C38    fadd_fwd        an extra mr to keep the object alive across a float load
827618E8    wstr_compare    loop rotation; counts kept in callee-saved r30/r31
821A5350    m_state_1or2    4/8  the boolean stays in r11 and is zero-extended at the end
825FE880    m_ctor_94       8/12 the vtable store will not move to the front
826973C8    b_strcmp        9/11 the CR FIELD of one compare -- cr6 against cr0
8215ED28    b_bounds_at    11/12 `lwzx` operands swapped; the same address either way
82606EC8    f_arena_alloc  33/40 the operand order of two `add`s
82202D08    m_pick_slot     2/11 folds a guard into beqlr where the target keeps one exit
82155080    e_normalize4   45/51 two `fmuls` operand slots
8214F7E8    e_axis_project  1/47 not close; the load census is recorded in the source
821A5270    m_copy_adjust   0/10 the copy is integer, so a type here is not float
```

Two of these were measured hard enough to be worth reading before trying
again. `826973C8`: `/O2` gets the CR field right and the GPRs wrong, `/O2
/Os` gets the GPRs right and the CR field wrong, and 2,304 flag combinations
produce nothing better than either. `82606EC8`: the `add` operand-order rule
in MATCHED.md says the base has to be READ earlier in the source, but any
source that reads it earlier keeps it live across the if-body, so MSVC hoists
instead of sinking and the body drops from 40 words to 37.

`8215E5B0` was on this list with its size recorded as 156 bytes, which is
wrong -- one `.pdata` unwind row covers six frameless thunks there (§7q).
That alone explains why objdiff scored it 12.1%, the worst of the fourteen.
Correcting the extent did not match it, but it did mean the thing being
measured was finally the right thing.

Two things are known about this class that were not. Translation-unit context
does NOT affect codegen -- the same function compiled six ways with different
company gives byte-identical output (§7m) -- so reconstructing whole files
would not help. And `tools/permuter.py`'s seven mutations do not reach
register allocation, so **the next mutation to write is one that changes
register PRESSURE**, not statement order.

**Larger, in rough order of value:**

1. **Havok**. 322 of 329 RTTI classes are `hk*`, so Havok is a large fraction
   of the remaining 60.6%. A matching **Havok 6.5.0-r1 for Xbox 360** would let
   `libmatch.py` eat it directly. The SDK has not turned up; RTTI identifies it
   without one, which is enough to exclude it from scope but not to link it.
2. **Scaleform GFx 3.x and FMOD Ex** for Xbox 360 — same idea.
3. ~~**VMX128 in Ghidra.**~~ **MOOT.** Nothing in the pipeline uses Ghidra
   any more (`tools/discover.py` replaced it — see "Ghidra, measured and
   replaced"). The one place VMX128 still cannot be read is **objdiff**,
   whose PowerPC backend is 750CL-flavoured; `tools/disasm.py` reads it.
4. ~~**MSVC switch tables.**~~ **DONE — `tools/switches.py`.** Of 14,708
   `bctr` sites, 104 are switch dispatches and 95 are decoded. Neither form
   is a table of addresses, which is exactly why Ghidra mishandles them:
   there is only an offset and a base built from a `lis`/`addi` pair.
   The byte form (51 sites) jumps to `caseBase + 4 * byteTable[value]`; the
   halfword form (53) to `caseBase + halfTable[value]`. `caseBase` is the
   word immediately after the `bctr`.

   The useful result is a negative: of 2,571 recovered case targets, **none
   is listed as a function**, `.pdata` or discovery. `verify.py` now checks
   that on every run. It does not explain the 13 functions Ghidra lists and
   discovery does not — none of those is a case target either.
5. **Spot-check the weak attributions.** 604 functions are attributed by
   `srcpath`/`havok` alone — a reference the function *makes*, not a byte
   match — and none has been confirmed by hand. They are deliberately not
   merged with the 6,453 `lib` matches.
6. ~~**A build system and a progress dashboard.**~~ **PARTLY DONE.**
   `tools/build.py` is the build and `tools/objdiff_export.py` gives the
   dashboard. What is still missing is the LINK: undecompiled code is copied
   from the original rather than assembled from objects, so section layout,
   symbol ordering and inter-object padding remain unverified. Finishing that
   is the largest remaining structural task.
7. **A register-pressure mutation for `tools/permuter.py`.** Its seven
   mutations do not reach register allocation, which is what all six
   remaining stalls come down to.
8. **CI.** Nothing runs `tools/verify.py` on commit; it is run by hand.

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

---

## Scope, honestly — and the two objections worth taking seriously

Both of these came from someone who knows the scene, and both are largely
right. Neither is a reason not to do this, but pretending otherwise would be
worse than useless.

### "There is a reason they did a recomp for Unleashed instead of a decomp"

Correct, and the reason is scale. A static recompilation gets a playable game
in months by translating the shipped binary; a byte-matching decompilation
recovers source and takes years. **That is why `../ToS-Port` exists
separately** — it is the recomp, and it is the right tool for playability.
This tree is the other thing, and it should not be confused for a faster
route to the same place.

The numbers, so nobody has to guess:

```
functions that are probably the game's own    22,392
their total size                           5,096,224 bytes
matched                                          145 functions, 6,248 bytes
                                                0.65% by count
                                                0.12% by BYTES
```

That is up from 64 functions and 1,508 bytes in one session — the count more
than doubled and the byte figure more than quadrupled, because ranking
candidates by CALLER COUNT rather than by size puts 40- to 170-byte shared
routines in front of 16-byte accessors.

It still gets harder from here, because the easy work is taken first:

```
mean matched function        40 bytes
median unmatched function   112 bytes
2,013 unmatched functions of 512+ bytes hold 49% of the remaining bytes
```

So: 140 matches is a working pipeline, a validated toolchain and a growing
body of compiler-behaviour knowledge, not a dent in the program.

### "Better luck doing it for Wii — symbols actually exist for that"

Also correct, and worth acting on rather than arguing with. GameCube/Wii
titles frequently shipped with a `.map` on the disc or DWARF in the `.dol`,
which is exactly why that scene's decomps are so far ahead. Nothing of the
kind exists here: this image has no symbol table, and every name in `src/` is
invented.

**What a Wii build would give this project, if it shares the codebase:**

* **Struct layouts and field offsets.** Both targets are 32-bit big-endian
  PowerPC, and a cross-platform engine shares its layouts. Our single largest
  source of guesswork is `char unk0000[0x84]` padding, and a symbol table
  would replace it with fields.
* **Function and class names**, transferable by structure — call-graph shape,
  string references, vtable order — even though the bytes differ.
* **Translation-unit boundaries**, which is the gap `tools/segment.py`
  measured and could not close: adjacency alone gives 55% precision, and a
  `.map` would give the answer outright.

**What would NOT transfer:** the bytes. Wii is Metrowerks CodeWarrior on
PowerPC 750CL; this is MSVC 15.00.8153 on Xenon. None of the 64 matches, none
of the register allocation, none of the instruction scheduling. A Wii decomp
and this are different programs that share a design.

**The evidence that the codebase IS shared** is in the image already. Assert
strings recovered by `tools/srcfiles.py` name the game's own build tree:

```
c:\branches\SB09\main\NG\Source\Engine\Graphics\Builder.cpp
c:\branches\SB09\main\NG\Source\Engine\System\Tasking.cpp
c:\branches\SB09\main\NG\Source\Engine\UI\Font.cpp
                        ^^ platform branch
```

`SB09` is the title, `NG` is Heavy Iron's "Next Gen" engine, and the platform
sits at a branch level above `Source/Engine/` — which is what a shared
codebase with per-platform branches looks like. If a Wii build carries symbols
for `Source/Engine/...`, those names describe the same classes this image
contains.

**So the practical answer to both objections is the same:** if a symbol-bearing
Wii build of this title can be had, getting it is worth more than any number
of hand-matched functions here, and it would be used to name and lay out —
not to match.

---

## Licensing

`thirdparty/disasm/` is binutils-derived (GPL). `game/` and `SDKFiles/` are
gitignored and are not distributed with this repository.
