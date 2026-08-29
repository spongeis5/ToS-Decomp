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
functions known                25,737     (.pdata 21,238 + Ghidra 4,499)
.text disassembled             97.3%      (2,060,734 of 2,116,991 words)
attributed as NOT the game's    7,611     (39.4% of .text)
remaining to decompile         18,126     (60.6%)
FUNCTIONS MATCHED                   1
```

The one match is `sub_822607F0` — see `MATCHED.md`. Verify it with:

```bash
python tools/match.py src/grid_indices.cpp 822607F0
```

That command working end to end is the project's heartbeat: it means the
image, the inventory, the XDK compiler and the comparison are all intact.

### What is attributed, and how strongly

| signal | functions | strength |
|---|---|---|
| `lib` | 6,332 | byte-for-byte match against an XDK library object — strong |
| `rtti_havok` | 673 | Havok vtable recovered from MSVC RTTI |
| `srcpath` | 352 | the function references a middleware source path — weak |
| `havok` | 251 | pushes a Havok profiler timer name — weak |
| `game_profiled` | 3 | pushes a `Ttz` name, so it IS the title's own |

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

**3. Ghidra 12.x** with `analyzeHeadless`. Only needed to rebuild the call
graph and the extra 4,499 functions.

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
| `vmx128_*.py` | four independent VMX128 validations — see `VMX128.md` |
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

**FIRST: settle whether the retail build used LTCG.** This decides whether the
matching loop this project is built around can work at all for the game's own
code, so nothing else is worth doing until it is answered.

Three functions have now been attempted. All three reached the exact byte
size with the right instructions and stalled on **instruction scheduling** —
where the compiler places one instruction — which no source shape and no flag
combination reached:

```
822607F0  120 B   MATCHED 30/30
82806FD0   84 B   11/21   8 source shapes, 65 flag combinations
826C1480   76 B   13/19   branchless, so NOT a branch-layout problem
827618E8  136 B   partial
```

`/GL` was tested directly and produces a machine-0000 object with **zero
PowerPC code bytes** — under LTCG, codegen happens at link time and there is
nothing in the object to compare. If the game's own translation units were
built that way, object-level matching cannot work and the unit of comparison
must become the linked image.

Evidence is split and neither side settles it. 6,332 functions match
**non-LTCG** XDK library objects byte for byte, which proves those libraries
were not regenerated — but `/LTCG` regenerates `/GL` objects while leaving
precompiled libraries alone, so it says nothing about the game's code.

The test to run: build a two-function object, link it with and without
`/LTCG`, and locate the code through the PE section table. A first attempt
compared raw file windows and scored 0/19 for BOTH arms, including the
non-LTCG build known to score 13/19 at object level — so that harness was
wrong, not the answer. `link.exe` works (see the manifest fix above).

**Then: match a second function.** `python tools/candidates.py` gives 2,565
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
- Which XDK lib variant the retail build linked (LTCG or not) is NOT_MEASURED.
- The `2909` toolchain in the Rich header is a prebuilt third-party library,
  unidentified.

---

## Licensing

`thirdparty/disasm/` is binutils-derived (GPL). `game/` and `SDKFiles/` are
gitignored and are not distributed with this repository.
