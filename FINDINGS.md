# ToS-Decomp — what is established

A byte-matching C++ decompilation of *SpongeBob's Truth or Square* (Xbox 360).

Separate project from `../ToS-Port`, which is a static-recompilation PC port of
the same title. Nothing here depends on that tree.

**Every entry states how it was measured.** A fact and the conditions it was
measured under are one thing, not two. `NOT_MEASURED` means exactly that — it is
not zero, and it is not a benign default.

---

## 1. The container

`game/DEFAULT.XEX`, 10,334,208 bytes, md5 `9d8fe3d885e1b4c8fe33f6e95fa0e255`.

```
entry point   822F8BC8      image base    82000000
image size    00AC0000      encryption    normal (AES-128-CBC)
                            compression   basic (data/zero runs — NOT LZX)
```

`python tools/xex.py` unpacks it to `build/default.pe.exe`, 11,272,192 bytes,
md5 `1403fa232c718295487e7fe936c8de1b`.

**Basic compression is the lucky break.** It is run-length data/zero block pairs,
so no LZX decompressor was needed — the whole unpacker is one file with a
self-contained AES-128 and no hard dependency.

**Three independent size derivations, and they were not all right the first
time.** The block table's data sizes sum to `0x9D8000`, which is exactly the file
bytes after the `0x3000` header. The block table's data+zero sums to `0xAB8000`.
The security info declares `0x00AC0000`, and 172 page descriptors × 64 KiB is
also exactly `0x00AC0000` — and the descriptor array's own extent reproduces the
security header size exactly (`0x184 + 172×24 = 0x11A4`).

So the block table describes *content* to `0xAB8000` and the image is rounded up
to whole 64 KiB pages; the trailing `0x8000` is page padding. The unpacker
zero-fills it and **refuses any shortfall of a page or more**, because that would
be a missing block rather than rounding.

A first pass at this asserted the block table summed to `0xAC0000` — arithmetic
done by eye, and wrong. The guard in the tool caught it. The page-descriptor
derivation is what settled it, and it has nothing in common with the block table.
*measured 2026-08-28 by `tools/xex.py`*

The retail key decrypts it (the devkit arm is tried too and is not what works);
session key `739167aec12d3de88f411d4c5db38c70`.

---

## 2. The toolchain — what a matching build has to reproduce

This is the load-bearing section for a *matching* decomp.

```
PE machine        01F2  POWERPCBE          PE32, section align 0x10000
linker version    9.0                      (optional-header field, direct read)
link timestamp    4A9243B2 = 2009-08-24 07:39:30 UTC
```

**The Rich header, decoded (xor key `8BB1AB7D`, 26 dwords from `DanS`):**

| prodid | build | count | reading |
|---|---|---|---|
| 0 | 0 | 2 | padding |
| 123 | **2909** | 2 | a second, older toolchain |
| 109 | **2909** | 1 | " |
| 1 | 0 | 324 | import descriptors |
| 138 | **8153** | 54 | |
| 149 | **8153** | 25 | |
| 147 | **8153** | 5 | |
| 131 | **8153** | 359 | one compiler front end |
| 132 | **8153** | 1090 | the other, and the bulk of the image |
| 146 | **8153** | 1 | |
| 145 | **8153** | 1 | conventionally the linker (last entry) |

**Build `8153` is the number to verify a candidate XDK against.** Every object
the title's own compiler produced carries it. 54+25+5+359+1090+1+1 = **1,535
objects** from that toolchain, of which the two large buckets (1,090 and 359)
are ~1,449 translation units.

> **CORRECTED IN §7l — read that instead of the "reading" column above.**
> Every product id has since been MEASURED against this XDK by building known
> flag combinations: 131 = C, 132 = C++, 137 = C `/GL`, 138 = C++ `/GL`,
> 145 = linker, 146 = export descriptor, 147 = XEX import descriptor,
> 149 = PowerPC assembler. Two readings below are wrong and are kept for the
> record: `2909` is **not** third-party (it is `xboxkrnl.lib`), and the
> `1 / 0 / 324` row is sixth in the file, not fourth.

~~The `2909` pair is a prebuilt third-party library — consistent with the
`BINK`/`BINKCONS`/`BINKDATA`/`BINKBSS` sections, which RAD ships as objects.~~

**The prodid → tool NAMES are NOT_MEASURED.** The community prodid table was not
consulted; naming 132 "the C++ compiler" from memory is exactly the move that
produces a confident wrong name. The *counts and builds* are read directly and
are what the verification needs.

> **Right caution, wrong conclusion.** Declining to *look up* the names was
> correct. Concluding they were therefore unavailable was not — the toolchain
> that stamps them is in `SDKFiles/`, so the meaning is measurable. See §7l.

**XDK version, from the image's own strings:**

```
82048FD8  e:\xenon\mar09\core\private\xtl\graphics\...   (53 XTL source paths)
8201C38C  Microsoft (R) Xbox 360 Shader Assembler 2.0.8276.0
```

So the statically-linked XTL comes from the **March 2009** XDK branch and a
component of it stamps **2.0.8276.0**. Whether `8153` and `8276` are the same
XDK release is NOT_MEASURED — they are different components and were read from
different places.

### FOUND AND CONFIRMED 2026-08-28: XDK 8276 (March 2009), the DEFAULT compiler

`SDKFiles/XDKSetupXenon8276.exe` is a self-extracting CAB; the toolchain is
extracted to `SDKFiles/xdk/XDK/`.

**The XDK ships TWO PowerPC toolchains, and only one matches.** This is a
discrimination rather than a coincidence — the wrong one was available in the
same folder and was rejected on evidence:

| toolchain | version | matches the image? |
|---|---|---|
| `XDK\bin\win32\` cl, c1, c1xx, c2 | **15.00.8153** | **yes** — Rich header build 8153 |
| `XDK\bin\win32\link.exe` | **9.00.8153** | **yes** — PE linker version 9.0 |
| `XDK\TechPreview\Mar09Compiler\` | 15.00.8327 | no |

```
Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 15.00.8153 for PowerPC
```

**End-to-end, not just a banner.** Compiling a trivial `.cpp` with it produces a
COFF object with machine `01F2` POWERPCBE and the symbol

```
@comp.id  value 00841FD9   -> low half 0x1FD9 = 8153
```

So the toolchain runs on this machine and stamps the same build the shipped
game's Rich header records. Three agreeing facts — Rich header, linker version,
and a live compile — from sources with nothing in common.
*measured 2026-08-28*

~~**The `2909` pair in the Rich header is still NOT_MEASURED.** It is a prebuilt
third-party library and is not from this XDK.~~
**MEASURED, and both halves of that sentence are wrong** — it *is* from this
XDK, `xboxkrnl.lib`, which every title links. §7l.

~~**Which lib variant the retail build linked is NOT_MEASURED.**~~ **ANSWERED
in §7l: the non-LTCG variants.** `XDK/lib/xbox/` carries 117 libs, 1.6 GB,
including `ltcg` variants beside ordinary ones. Nine matched libraries have an
LTCG twin and in all nine the ordinary variant is the one that matched
byte-for-byte; the two cannot both be linked. The PDB path naming the
configuration `Xbox 360MasterWAD` did suggest LTCG — an inference from a folder
name, and it did not survive measurement.

### Operational: MSVC flags and Git Bash

Git Bash rewrites `/c` and `/nologo` into Windows paths before `cl.exe` sees
them. Invoke the XDK tools from PowerShell, or with `MSYS_NO_PATHCONV=1`.
Found by watching `cl` warn about a source file called `C:/Program Files/Git/nologo`.

---

## 3. The original source tree

Read out of the image's own assert/`__FILE__` strings.

```
c:\branches\SB09\main\NG\Source\Engine\System\CoreTasking.cpp
c:\branches\SB09\main\NG\Source\Engine\System\Tasking.cpp
c:\branches\SB09\main\NG\Source\Engine\System\Time.cpp
c:\branches\SB09\main\NG\Source\Engine\UI\Font.cpp
c:\branches\SB09\main\NG\Source\Engine\Graphics\Builder.cpp
c:\branches\SB09\main\NG\Source\Engine\Graphics\BuildMemory.cpp
c:\branches\SB09\main\NG\Source\Engine\Graphics\Display.cpp
c:\branches\SB09\main\NG\Source\Engine\Graphics\Effect.cpp
c:\branches\SB09\main\NG\Source\Engine\Graphics\Sampler.cpp
c:\branches\SB09\main\NG\Source\Engine\Graphics\Scene.cpp
c:\branches\sb09\main\ng\source\tools\fmod_source\src\src\fmod_memory.h
```

and the build's own PDB path, which names the configuration:

```
c:\branches\SB09\main\GM\Engine\Tmp\BuiltOutput\SB09\Xbox 360MasterWAD\SB09MasterWAD.pdb
```

So: branch `SB09/main`, engine source under `NG/Source/Engine/{System,UI,Graphics,...}`,
built as configuration **`Xbox 360MasterWAD`**. `NG` is the engine (Heavy Iron's
next-gen engine); `GM` is the game side.

**These 11 are the files that happen to carry a path string — not the source
tree.** 1,449-odd translation units exist and 11 are named. This is a floor on
what is known, not a map.
*measured 2026-08-28, `tools/strings.py`, a whole-image census of 46,688 strings
classified into 10 buckets whose counts are asserted to sum to the total*

### Middleware identified

| | version | evidence |
|---|---|---|
| Bink | — | 4 dedicated PE sections |
| Scaleform GFx | — | `ScaleformPlayer`, `ScaleformTexture` |
| FMOD | — | `fmod_memory.h` path, `.fev`/`.fsb` assets |
| Havok | `6.5.0-r1` | version string; older ones present as a compat table |
| libFLAC | `1.2.1 20070917` | banner string |

Matching those is a different problem from matching the game: they are
third-party libraries, and a matching decomp normally links the original
binaries rather than reconstructing them.

---

## 4. Sections

RVA equals raw offset for the first three sections only; the rest are
repositioned, so the unpacked file is **not** a flat memory image and a loader
must use the section table.

```
  name       vsize     va        rawptr    RVA==raw?
  .rdata    000C5914  82000600  00000600  yes
  .pdata    000297B0  820C6000  000C6000  yes
  BINKCONS  00002920  820EF800  000EF800  yes
  .text     008135FC  82100000  000F2200  no
  BINK      0000FA98  82913600  00905800  no
  BINKBSS   000003B8  82930000  --------  bss
  .data     0013467C  82930400  00915400  no
  BINKDATA  00003D68  82A64C00  00970600  no
  .tls      0000003D  82A68A00  00974400  no
  .XBMOVIE  0000000C  82A68C00  00974600  no
  .edata    00001A82  82A70000  00974800  no
  .idata    000003E2  82A80000  00976400  no
  .XBLD     000000C0  82A90000  00976800  no
  .reloc    00069C90  82A90200  00976A00  no
```

`.text` is `0x8135FC` = 8,468,988 bytes of code at `82100000`.
`.pdata` is `0x297B0` = 169,904 bytes = **21,238 unwind rows**, which is the
compiler's own function table and the best available function inventory.

`.XBLD` is **not** link metadata — it is 192 bytes of float data. Checked, because
the name invites the other reading.

---

## 5. The matching loop works; the linker does not

`cl.exe /c` runs correctly on this machine and emits one COMDAT section per
function, which is exactly the granularity per-function matching needs:

```
?lerp@@YAMMMM@Z   section .text  +0x0..0xc  12 byte(s)
    00000000  ec020828  fsubs  f0, f2, f1
    00000004  ec2008fa  fmadds f1, f0, f3, f1
    00000008  4e800020  blr
```

`tools/objcode.py` extracts and disassembles that. Capstone has no VMX128, so a
word it cannot decode is printed raw with a marker rather than skipped — a
disassembly that silently drops instructions is a disassembly of a different
function.

**`link.exe` does not start: exit `0xC0000142` STATUS_DLL_INIT_FAILED.** Tried
with the CWD set to its own directory and with every dependency present
(`mspdb80`, `mspdbcore`, `msobj80`, `msvcr80/90`, `msvcp80/90` are all beside
it, and the VC80 CRT is installed system-wide). `cl.exe` from the same folder
runs fine, so it is specific to the linker's DLL set.

This does **not** block the work. The matching loop is compile-and-compare and
needs only `cl.exe`; the linker is needed to produce a whole image, which is a
long way off. `imagexex.exe` (PE → XEX) is present for when it matters.
*measured 2026-08-28*

---

## 6. Ghidra

Project at `ghidra/ToS`, language `PowerPC:BE:64:A2ALT-32addr` (64-bit PowerISA
with Altivec, 32-bit addressing — Xenon).

**The PE loader reported "Import succeeded" while parsing the import directory
as noise** (dozens of `Invalid RVA`) and refusing the exception directory
outright (`unsupported architecture: 0x1f2`). Neither matters — XEX imports live
in the XEX header, not `.idata`, and `.pdata` is parsed by `tools/pdata.py` here.

**The memory map was checked rather than believed.** `ReportBlocks.java` hashes
the first bytes of every block and `tools/verify_ghidra.py` recomputes the same
hash from the file: **14 of 14 initialised blocks agree, at the right addresses**,
1 uninitialised (BINKBSS) not checkable. So the map is sound even though the
loader was noisy.

A first run of that check reported 2 disagreements. Those were a defect in the
CHECKER — it hashed a fixed 256 bytes from the file against Ghidra's hash of a
61-byte and a 12-byte block, comparing two different populations.

`ApplyPdata.java` created **20,674** functions of 21,238 rows; 2 already existed,
0 not in memory, 0 `createFunction` failures, **562 disassembly failures**, and
the arms sum to 21,238 exactly.

---

## 7y. Two functions matched from opposite ends are the same container

*measured 2026-08-29*

`sub_82667E58` and `sub_82667EE0` were matched by climbing up from
`BinAlloc`/`BinFree`, and modelled as a growable vector:

```
+0  void* data
+4  s32   count
+8  s32   capacityAndFlags     bit 31 = "not ours to free", low 30 = capacity
```

`sub_8267ACC0` was matched later, chosen for STRUCTURE rather than for
throughput -- 52 fields written in 236 bytes. Its vtable at 8206BE70 is named
by MSVC RTTI as `.?AVhkpWorld@@`, and sixteen of those fields are 12-byte
triples initialised to `{0, 0, 0x80000000}`.

**Those are the same type.** A pointer, a count, and a capacity whose top bit
is a don't-deallocate flag is `hkArray`, Havok's array. So the "vector" is
hkArray's growth path, and the constructor is sixteen empty ones.

Neither match knew what it was. They were reached by different means -- one by
following the call graph up from an allocator, the other by ranking every
unmatched function by how many field offsets it would pin -- and they met in
the middle. **That is the first structural fact this project has recovered
from two independent directions**, and it is the kind of confirmation a
per-function match cannot give on its own.

It also means `src/m_bin_free.cpp`'s BinAlloc and BinFree, 206 and 420
callers, are what Havok's memory allocator resolves to in this build.

---

## 7z. Scope, corrected: attributed is not the same as out of scope

*2026-08-29*

The attribution table used to run two different things together. Separated by
whether the archive is HELD:

```
                                  functions      bytes   share of .text
the game's own (unattributed)        23,574   5,193,648    61.4%
XDK library, WE HAVE THE .lib         6,523   2,713,724    32.1%
Havok, SDK NOT HELD                   1,429     341,792     4.0%
other middleware, NOT HELD              353     198,132     2.3%
game, profiler-named                      3       7,700     0.1%

linkable from libraries we hold                2,713,724    32.1%
MUST BE WRITTEN to ever link                   5,741,272    67.9%
```

The `lib` bucket is genuinely out of scope: `xgraphics.lib` (2,459
functions), `d3dx9.lib` (1,948), `xaudio2.lib` (737), `d3d9.lib` (466),
`xapilib.lib`, `libcMT.lib`. Those ship with the XDK and the linker can be
handed them.

Havok, FMOD, Ogg Vorbis and Bink are 539,924 bytes with no archive to link.
Identifying them by RTTI or by a source-path string says what they ARE; it
does not make them go away. Excluding them from the COUNT was reasonable --
they are not the title's own work -- but excluding them from the PLAN was
not, and the plan is what matters for a decompilation whose goal is a
linkable image.

**And they may be the easiest 6.3% in the file.** 1,178 Havok functions
arrive with their class name already recovered from RTTI, against a published
API whose headers and hierarchy are documented. Game code arrives with
nothing but its bytes. `sub_8267ACC0` was matched from a standing start
because RTTI named the class and three constants named the member type.

---

## 7w. Names are RECOVERABLE for about a hundred functions

*measured 2026-08-29*

`sub_82216918` is in `src/manifest.txt` as **`TtCheckLineOfSight`**, and that
is the game's own name, not an invented one. The function pushes the string
at 8200BA04 into the profiler, and that string is its name.

`tools/profnames.py` has recovered 100+ of these all along; nothing had used
them to NAME anything:

```
821D3D48  TtzCam2Player_update                 3716 B
82241BF0  TtzNPCSteering_ApplySteering_hover   2228 B
822CA548  TtzSceneUpdate_CheckingTransparent   2376 B
8263EE68  TtcheckSupportWithCollector          3016 B
82216918  TtCheckLineOfSight                    304 B   <- matched
82667170  TtSetSurfVel                          384 B
826731D8  TtrcSphere                            144 B
82677BC0  TtWatchDog:FreeMem                    352 B
```

HANDBOOK.md gap 4 says "names are invented, not recovered". For this population
that is no longer true, and the name says what a function is for before a
line of it is read -- `TtcheckSupport` is the character-grounding test,
`TtSetSurfVel` is surface velocity for moving platforms, `TtCheckLineOfSight`
is the AI visibility test.

**The profiler scope is an inlined six-instruction macro**, and recognising
it is most of the work on any `Tt*` function. Push at entry with the name,
push again before returning with `"Et"` at 820074E4 -- end of timer:

```
lwz   r31,0(r13)         the thread block
li    r30,48
lwzx  r10,r30,r31        t_profiler -- the __declspec(thread) READ form
lwz   r3,12(r10)         end
lwz   r9,4(r10)          cursor
cmplw cr6,r9,r3 ; bge-   skip when the buffer is full
stw   r6,0(r9)           the NAME
mftb  r5                 the time base
stw   r5,4(r9)
addi  r7,r9,12           entries are 12 bytes
stw   r7,4(r10)
```

So the buffer is `{ char unk[4]; Entry* cursor; char unk[4]; Entry* end; }` in
TLS slot 48, and an entry is `{ const char* name; u32 stamp; u32 unk; }`.

---

## 7y. The first real LINK — and four facts about `link.exe` 9.00.8153

*measured 2026-08-29*

Everything before this was a **splice**: `build.py` compiles each function,
resolves its relocations itself, and writes the bytes at the address the
manifest names. A splice cannot see whether two functions PACK, what is in
the PADDING between them, or whether the ORDER is reachable at all.

`tools/link.py` now hands contiguous **runs** of matched functions to the
retail linker, ordered by `/ORDER:@` and placed at their retail addresses.

```
runs of 2+ adjacent matched functions          152      13,116 bytes
  linked, placed and byte-identical            124      10,472 bytes
  blocked: relocate against unwritten code      26       2,584 bytes
  blocked: one .cpp at two /O levels             2          60 bytes
```

Six negative controls, five of which must report a difference: order
reversed, first two functions swapped, placed 8 bytes early, placed 8 bytes
late, compared 4 bytes off.

### The four measured facts

**1. `link.exe` refuses a REL24 against an ABSOLUTE symbol.** The obvious way
to satisfy a call to a function nobody has written is a COFF symbol with
`SectionNumber = IMAGE_SYM_ABSOLUTE` at the address read out of the image.
It does not work: `LNK2013: REL24 fixup overflow` at **12 of 12** values
swept, from `00000000` to `FFFFFFFC`, *including* values pointing at the
linked code itself. It is the symbol kind it objects to, not the distance. So
an undecompiled callee has to be **placed** in a real section — there is no
stub shortcut, and that is why 26 runs are blocked.

**2. Every function COMDAT `cl.exe` emits is `IMAGE_SCN_ALIGN_8BYTES`**, and
that predicts the gaps between matched functions. Of the 464 consecutive
manifest pairs whose gap is 0..4, the gap is 4 **exactly** when the previous
function ends at 4 mod 8 — 464 agreements, 0 disagreements — and all 298
non-zero gaps are filled with zero. A 4-byte gap is not a hole in what is
known; it is the linker's own arithmetic, and reproducing it is now checked.
It also means a run starting at 4 mod 8 could not be placed at all. None
does, and `link.py` checks rather than assuming.

**3. `/ORDER` orders COMDATs and nothing else.** Placing a run at an address
like `821C7C60` needs a padding block ahead of it, because `/BASE` must be
64K-aligned and `.text` lands at a 64K-aligned RVA. Written first as an
ordinary `.text` section and named first in the order file, the pad was
placed **after** all 55 ordered functions — at exactly +0x36C, past the end
of the run — and the run began at offset zero. As a COMDAT it lands first and
the run hits `821C7C60` exactly. Two links are needed: one to measure where
`.text` went, one to place.

**4. 67 of 406 translation units reference `_fltused`, and 0 relocate against
it.** `cl.exe` emits the reference from any unit touching floating point so
the linker drags in CRT FP support. Defining it is safe *because* no
relocation names it — a symbol no relocation names cannot put a byte
anywhere. That justification is read off each object, not from a list of
names, because a list would also excuse the next symbol that looked similar
and did have a fixup.

### What it does not establish

The runs are fragments. Nothing here links two runs together, and the 26
blocked runs are blocked on exactly the thing a whole-image link would have
to solve. Only 34,096 of 8,467,964 `.text` bytes are built at all; a link
spanning more would still be **copying** the filler, so what it adds is that
the layout and the relocations become the linker's work rather than ours --
not that more of the program has been recovered.

### And the next step is NOT 26 bigger runs -- measured

The obvious extension is to widen each blocked run until it contains what it
calls. Resolving all 79 references the 26 blocked runs need, out of the retail
bytes, says not to bother:

```
where the needed symbols live         .text, a function start        39
(76 of 79 resolved; 3 could not be     .data                          20
 solved from the retail word alone)    .rdata                         14
                                       .text, NOT a function start     3

span that would have to be laid out    smallest                      44 B
per blocked run, lowest thing it       median                   4.10 MB
needs to highest                       largest                 10.13 MB
                                       under 64 KB              4 of 26
```

The median blocked run would have to lay out **4.1 MB** to reach what it
calls, and the largest 10.13 MB -- more than `.text` itself, because a third
of the references are to `.data` and `.rdata`, which sit after it. Twenty-two
of twenty-six are not local problems at all.

So the step after this one is **one link of the whole image**, with `.text`,
`.rdata` and `.data` all placed and everything undecompiled going in as
filler COMDATs carrying the retail bytes -- not twenty-six wider runs. That is
a different size of job, and it is worth knowing before starting the small
version of it.

**Two loose ends worth a look first.** Three references land in `.text` at
something that is not a function start -- either a jump table, an in-section
constant, or an inventory gap. And three could not be solved from the retail
word at all. Neither is explained.

---

## 7x. build.py can check that one retail function has ONE name

*measured 2026-08-29*

Adding `sub_82662E08` to the manifest surfaced this: `src/null_tailcall.cpp`
declared `void Use(void*)` and tail-called it, and that call resolves to
82662E08 -- which is now matched as `u32 ReleaseHandle(u32)`. One retail
function, two invented names, and two declarations that disagree about the
ARGUMENT TYPE.

That is not cosmetic. Argument count and type change codegen, so if both
files are believed then one of them is matching for the wrong reason. It only
becomes visible once the callee is matched too, which is why it has to be
checked on every build rather than once.

`build.py` already resolves every REL24 to a retail address, so the check is
free: a call landing on an address the manifest covers should be calling that
manifest row's symbol. Seven such calls exist today; the `Use`/`ReleaseHandle`
one was a genuine type error and is fixed -- the field is a HANDLE, not a
pointer -- and `null_tailcall.cpp` still matches with the corrected type. The
rest are aliases for functions matched later in the session and are harmless,
but they are on the record rather than invisible.

This is the same class as the "one symbol resolves to two addresses" check
already here, from the other direction: that one catches one name with two
addresses, this one catches one address with two names.

---

## 7u. A "function start" control can FALL INTO is not a function start

*measured 2026-08-29*

`sub_8262F658` has **420 callers**, runs 164 bytes to 8262F6F8, and the
inventory records it as **68**. The reason is that 8262F69C is listed as a
function start. It is not one: the word before it is `lwzx r11,r9,r3`, so
control falls into it. It is the label where a switch's two index
computations join.

Discovery's branch sweep takes the target of any unconditional `b` as a
function start, and from the sweep's point of view an intra-function jump is
indistinguishable from a tail call -- it has no function extents yet.

**The test, and its control.** "Is the word before this address an
unconditional terminator, or padding?" Run over the whole inventory, split by
which source supplied each row:

```
source        rows    reachable by FALL-THROUGH
pdata        21238        86   ( 0.4%)   <- the CONTROL: the compiler's own
addrtaken     1252         0   ( 0.0%)      function starts
discover      9392      1328   (14.1%)   <- 35x the control
```

The `.pdata` figure is what "wrong" looks like on this instrument, because
those are the compiler's own answers. 0.4% is the noise floor; 14.1% is a
defect. And the address-taken source scoring 0.0% is a third, independent
confirmation that it is clean (7r).

1,414 rows are truncated by a suspect start, holding 125,508 bytes that
belong to the row before them.

**Only 2 of 5,020 match candidates are affected**, because `candidates.py`
requires a terminator at the end and a truncated row rarely has one. The
damage lands almost entirely on NON-leaf functions -- which is exactly the
population that ranking by caller count surfaces, and where the most-called
functions in the image live.

**What was done.** `match.py` now bounds its size reconciliation by the next
start that is not fall-through reachable, and `can_extend` has been split out
with five tests of its own. The inventory itself is NOT changed: that would
move 1,414 sizes at once and make every derived file stale, and the
reconciliation removes the harm where it is felt.

**A second bug in the same place.** The reconciliation compared the extra
words as RAW BYTES, so it could never fire for a function whose tail holds a
relocation -- and a tail call is exactly that. `sub_8262F658` matched 17 of 17
words and still reported SIZE DIFFERS. That is the same mistake `can_shrink`
already documents, made twice in the same file.

---

## 7v. A THIRD switch form: tables of absolute addresses

*measured 2026-08-29*

7n says of the two decoded forms: "Neither of THESE TWO forms is a table of addresses (a third form is, and it is the most common -- see 7v),
which is exactly why Ghidra mishandles them." **That was a statement about
the decoder, not about the image.** There is a third form, and it is the most
common of the three:

```
addi   r11,r3,-27          bias the switch value to zero
cmplwi cr6,r11,24          the bound
bgtlr  cr6                 default
lis    r12,hi
rlwinm r0,r11,2,0,29       value * 4
addi   r12,r12,lo          = the table
lwzx   r0,r12,r0           the ADDRESS itself, not an offset
mtctr  r0
bctr
```

`find_dispatches` requires the exact tail `add rX,rBase,rOff ; mtctr ; bctr`,
because both forms it knew load an OFFSET and add it to a base. This one has
no `add`, so every dispatch of this shape was invisible.

```
form                       dispatches   case targets
lbzx, byte offsets                 51          (2,571 between them)
lhzx, halfword offsets             53
lwzx, ABSOLUTE ADDRESSES          333            2,084
```

**331 inventory rows are jump TABLES listed as functions.** A table follows
its dispatch, so the word before it is that dispatch's own `bctr` -- a
terminator -- and so it passes every "is this a real start?" test in the
project, including the one 7u introduces. Each one truncates the function it
sits inside: `sub_821A99F8` is 176 bytes with its table inline and the
inventory records 36.

`tools/switches.py` now writes `build/switch_tables.txt`, and `match.py`
excludes those ranges when bounding its reconciliation. That was worth two
matches immediately and it is general: any function containing a switch was
unmeasurable before it.

**And the tables are now VERIFIED rather than copied.** A jump table is a run
of `IMAGE_REL_PPC_ADDR32` relocations against compiler-generated labels
(`$LN5`, `$LN10`) with a zero addend, so the object records the case mapping
as symbol NAMES. `build.py` used to patch whole-word relocations straight
from the image, which would excuse the entire mapping: a source sending case
30 to the wrong arm verifies clean, because the bodies are identical and only
the table says which case reaches which. `coffreloc.LABELS` now exposes every
label's offset, so the entry the linker wrote is PREDICTABLE:

```
expected = target + (label_offset - function_offset)
```

All 25 entries of `sub_821A99F8` are predicted and agree. `verify.py` has a
negative control that moves one case to the wrong arm and requires the build
to fail; it reports the disagreement per entry, naming the label.

---

## 7t. The per-unit flag claim, measured over every match

*measured 2026-08-28*

7m established that the optimisation level is a property of the TRANSLATION
UNIT on the strength of six adjacent pairs. `tools/flagpairs.py` now compiles
every matched function at both levels and reports the whole picture:

```
118 matched function(s) classified
  /O2 only     51
  /Os only     16
  insensitive  50   <- carries NO evidence, excluded from the pairs

19 informative adjacent pair(s), 19 agreements, 0 disagreements
```

**Excluding the insensitive half is the discipline, not a convenience.**
Roughly 43% of matched functions compile identically at both levels -- small
accessors mostly -- and a pair count that included them would report
near-total agreement no matter what the truth was. It would be the same shape
as `.pdata` inflating the data-pointer hit rate (7f) and switch case bodies
inflating the address-taken count (7r): a population padded with items that
cannot disagree.

The longest run is the string routines, six consecutive functions and four
agreeing adjacent pairs:

```
82540728  StrLen         /O2 only
82540750  StrCopy        /O2 only   gap  40
82540770  StrCopyN       /O2 only   gap  32
825408B0  StrCompareN    /O2 only   gap 320
825408F8  StrCompareI    /O2 only   gap  72
82540968  StrCompareNI   /O2 only   gap 112
```

That is a translation unit identified by measurement rather than by guess,
and it is the first one this project has. Two `/Os` runs of the same shape
exist at `825E3598`/`825E35C8` and across
`82662F20`/`82663260`/`82663370`.

`flagpairs.py` prints any disagreeing pair in full and says in its own output
that such a pair would mean the claim needs rewriting rather than defending.
There are none, at 19 pairs.

**A false alarm worth recording.** `sub_827FE8A0` sits 152 bytes after
`sub_827FE808`, which is `/Os` only, and it matched at `/O2` -- which would
have been the first disagreement. It also matches at `/O2 /Os`. It is
insensitive, so it says nothing, which is exactly why the three-way
classification exists.

---

## 7q. One `.pdata` unwind row can cover SEVERAL functions

*measured 2026-08-28*

`sub_8215E5B0` is recorded as **156 bytes**. It is **28**. The other 128 bytes
are five more functions:

```
8215E5B0  lwz r10,0(r3) ; ... ; b 82602EA0      <- the function, 28 B
8215E5CC  .long 0                                  alignment
8215E5D0  lwz r10,32(r3) ; ... ; mtctr ; bctr   <- a second body
8215E5F8  ...                                   <- a third
8215E628  ...                                   <- a fourth
```

That row is one of the six stalls in MATCHED.md, and it had been compared
against 156 bytes the whole time -- which is why objdiff scored it 12.1%, the
worst of the fourteen.

**Where the wrong size comes from.** `.pdata` has rows at 8215E560 and
8215E650 and NOTHING between, so the 160-byte gap is not described by the
unwind table at all. These are frameless thunks: no prologue, nothing to
unwind, so the linker emits no row. `discover.py` found the first one through
the branch sweep and sized it extent-to-next-known-start.

**Why the other five were invisible.** Nothing branches to them and no data
word holds their address, so neither of discovery's two sources could see
them -- see 7r.

**How many rows are like this.** A conservative detector -- an unconditional
terminator, then ZERO padding (the linker pads COMDATs with zeros; MSVC pads
loop alignment inside a function with nops), landing on a 16-byte boundary,
with nothing inside the row branching there -- fires on **156 of 30,630 rows
(0.51%)**, and **123 of them are offered as match candidates**. That is a
floor, not a count: `82665388` holds two 8-byte bodies back to back with no
padding at all, and no pad-based detector can see that class.

**A wider rule was tried and rejected.** Dropping the padding and alignment
requirements and keeping only reachability gives 1,288 rows and 6,047 new
starts, and scores **60.0% precision / 58.3% recall** against the 36
byte-matched library starts that are currently hidden inside a larger row.
That is not good enough to change an inventory with. Worse, the ground truth
is itself noisy -- several of those "real starts" are preceded by a
CONDITIONAL forward branch, which is not a function-boundary shape -- so the
60% is not trustworthy either.

**Two earlier versions of this measurement were wrong, both in the
denominator.** The first scored precision as "predictions inside the
byte-matched address range", and that range is nearly the whole image, so
predictions in GAME code -- which the library matcher could never confirm --
counted as failures. It reported 1.1%. The second restricted to rows
containing a known library start, which is better but still assumes absence
from `lib_matches.txt` means "not a function", when it only means "not
matched". **Three attempts, three denominators, and only the third could
speak.**

**The structural rule DOES have independent support, found later.** Scoring
it against `lib_matches.txt` was hopeless -- only 3 of its 185 predictions
fell where that ground truth could speak. But the address-taken source (7r)
is a completely different derivation, and the two overlap:

```
second bodies found by the structural rule   185
  of those ALSO found by address-taken        60   (32.4%)
  found ONLY by the structural rule          125
address-taken starts                       1,252
  found ONLY by address-taken              1,192
union                                      1,377
```

60 independent confirmations is worth far more than the 2-of-3 that
`lib_matches` could offer, and the two sources being 96% disjoint says they
are finding different populations rather than the same one twice.

**And there is a fourth class that NEITHER finds.** The second bodies inside
the small merged rows -- `82665390`, `82666370`, `82677060`, `82639C70`,
`82697748` -- return **zero** references from all three scans: no branch, no
data word, no `lis`/`addi` pair. They are reachable by nothing in the image.
Only the structural rule finds them, and only because of where they sit. So:

```
called                  -> the branch sweep
in a vtable or table    -> the data-pointer scan
address formed in code  -> addrtaken.py            (7r)
referenced by NOTHING   -> only the shape of the row around it   (this section)
```

**What was done instead of changing the inventory.** `match.py` grew
`can_shrink()`: it cuts the comparison window to our code's length when, and
only when, our code ends in an unconditional terminator, the retail word
there is one too, no branch inside our code reaches the leftover range, and
every non-relocated prefix word agrees. Together those say the retail
function ends where ours does. It is a relaxation of a size check, so it has
six tests of its own (`tools/test_shrink.py`) of which **five must refuse**;
`verify.py` runs them. Four matches came straight out of it.

---

## 7r. A THIRD discovery source: addresses taken in code

*measured 2026-08-28*

`discover.py` had two sources and neither can see a function pointer that is
formed in code and stored through a register:

```
lis  r11, hi
addi r11, r11, lo        ; = 8215E5D0
stw  r11, 32(r3)
```

The branch sweep reads `bl`/`b`, so it finds what is CALLED. The data-pointer
scan reads words in data sections, so it finds what a vtable POINTS AT.
Neither sees the above. `tools/addrtaken.py` does.

```
scanned 2,133,029 instruction words; 55,914 lis sites
  9,077 ran to the 32-word lookahead bound without pairing (a BOUND)
distinct .text addresses formed in code            2,414
  MSVC switch case bodies, excluded                   89
  remaining                                        2,325
    already a known function start                 1,070  (46.0%)
    inside a known function but not its start        524
    in no known function at all                      731
```

**The switch exclusion is not cosmetic.** A switch dispatch builds `caseBase`
with exactly this `lis`/`addi` pair, so every one of the 2,571 case targets
`switches.py` recovered is an address formed in code and none of them is a
function. Before the exclusion, the first "new function" this reported was
82107B58 -- row one of `build/switch_targets.txt`. That is the same shape as
`.pdata` contaminating the data-pointer hit rate (7f), and it is the third
time in this project that a population has been inflated by including the
very thing being predicted.

**The independent check is CALIBRATED, which is the point.** The check is
that the word before the address is an unconditional terminator, which is
what a function boundary looks like and has nothing to do with how these were
found. Run first on the addresses that are already known starts, where the
answer is known:

```
known starts (calibration)   1,070   terminator before  1,066   99.6%
interior, unknown              524   terminator before    521   99.4%
outside any function           731   terminator before    731  100.0%
```

99.6% is what "yes" looks like on this instrument. The unknowns score the
same, which is the evidence.

**Result: 1,252 new function starts**, 521 of them interior to an existing
row (so those rows are too long) and 731 in no known function at all. Written
to `build/addrtaken.txt`.

**Available but NOT adopted**, as `python tools/inventory.py --addrtaken`.
Tested end to end:

```
rows           30,630  ->  31,882
rows SHORTENED because a new start falls inside them        341
.text covered by the inventory   8,432,420  ->  8,454,996 bytes  (99.6% -> 99.85%)
tools/build.py with that inventory:  exit 0, all 145 matches still OK
```

It is left opt-in for one reason: adopting it makes every derived file stale
at once. `attribute.py` and `candidates.py` would both have to be re-run in
that order -- running `candidates.py` against a stale attribution silently
hides thousands of candidates, which has happened here before -- and every
headline count in HANDBOOK.md would need re-measuring. That is a deliberate
step, not a side effect of a session that was doing something else.
`can_shrink` in match.py already removes the immediate harm, which is what
made it safe to defer.

Checked before proposing it: of the matched and attempted functions,
**exactly one** is affected -- `8215E5B0`, whose extent it corrects from 156
to 32.

**It does not subsume 7q.** It recovers the 8215E5B0 thunk family, which is
what prompted it, but of eight known hidden bodies it finds one. The second
bodies of the small merged rows are referenced by nothing at all and are
reachable only through the shape of the row containing them. The two sources
are 96% disjoint; neither replaces the other.

---

## 7s. IMAGE_REL_PPC_TOCREL14, measured rather than named

*measured 2026-08-28*

`build.py` refused `sub_82602F98` with "TOCREL14 at +0x0 is not handled". The
relocation is `__declspec(thread)`: r13 holds the thread block and the linker
fills in the slot offset.

The name says 14 bits. Rather than believe it, a probe with members at
offsets 0, 1, 2, 4 and 8 was compiled:

```
li   r11,0            <- TOCREL14 on the symbol, immediate ALWAYS zero
lwz  r10,0(r13)
addi r11,r11,1        <- the member offset, in a SEPARATE instruction
```

Five cases out of five. **The compiler never folds a member offset into the
relocated immediate**, so every bit of that D-form immediate is the linker's
and taking all sixteen from the retail word cannot mask anything the source
decided.

That reasoning depends entirely on the placeholder being zero, so it is
ENFORCED rather than assumed: `PLACEHOLDER_MUST_BE_ZERO` in `coffreloc.py`,
and `build.relocate` refuses when the field is non-zero. `test_coffreloc.py`
grew four TOCREL14 cases, one of which sets the placeholder and requires the
refusal.

The un-folded `li` is also the diagnostic when READING the image: two
ordinary literals would have folded into one `addi`. `li <slot>` + `lwz
rX,0(r13)` + `lwzx` reads a thread variable's value; `lwz` + `li` + `add`
takes its address.

---

## 7p. The game's own source tree, from its assert strings

*measured 2026-08-28*

`tools/srcfiles.py` recovers 188 source paths the code forms addresses to. Most
are middleware -- Havok's `./Collide/...`, FMOD's `../src/fmod_*.cpp`, Ogg
Vorbis, and 51 XDK paths under `e:\xenon\mar09\`. Ten are the title's own:

```
c:\branches\SB09\main\NG\Source\Engine\Graphics\BuildMemory.cpp   82909D80
c:\branches\SB09\main\NG\Source\Engine\Graphics\Builder.cpp       82909D00
c:\branches\SB09\main\NG\Source\Engine\Graphics\Display.cpp       82909E00
c:\branches\SB09\main\NG\Source\Engine\Graphics\Effect.cpp        82909EC8
c:\branches\SB09\main\NG\Source\Engine\Graphics\Sampler.cpp       8290A1C8
c:\branches\SB09\main\NG\Source\Engine\Graphics\Scene.cpp         8290A248
c:\branches\SB09\main\NG\Source\Engine\System\CoreTasking.cpp     829089B0
c:\branches\SB09\main\NG\Source\Engine\System\Tasking.cpp         82908B08
c:\branches\SB09\main\NG\Source\Engine\System\Time.cpp            82908BA0
c:\branches\SB09\main\NG\Source\Engine\UI\Font.cpp                82908E88
```

Three things follow.

**The engine is `NG`** -- Heavy Iron's "Next Gen" -- laid out as
`Source/Engine/{Graphics,System,UI}/`. `SB09` is the title branch.

**The platform sits ABOVE `Source/Engine/`**, which is the shape of a shared
codebase with per-platform branches. That is the concrete reason a
symbol-bearing Wii build would transfer names and struct layouts to this
image even though no byte would transfer.

**These ten are ground truth for translation units.** Each address is a
function that asserts from a named file, so ten TU labels are known outright
-- a much stronger signal than `tools/segment.py`'s adjacency clustering,
which scores 55% precision. They have not been used for that yet.

---

## 7m. Flags are a property of the TRANSLATION UNIT, not of the build

*measured 2026-08-28*

For most of this project's life `MATCHED.md` said every match was found at one
uniform `/O2 /Gy /GS- /fp:fast`, and offered that uniformity as evidence about
how the title was built. **It is wrong.** Eight functions recorded as stalls
were never stalls: they match at `/O2 /Os`, and at `/O1`, and not at `/O2`.

### The reasoning that hid it

`sub_827007E8` was found to match under `/Os` early, and the idea was
dismissed on this basis:

> its two nearest neighbours are the *identical idiom* and use `addi r10` â€”
> the `/O2` form. Same neighbourhood, both register choices.

Those neighbours are **8,736 bytes away**. That is not a neighbourhood and it
is not the same translation unit. A plausible sentence was allowed to stand in
for a measurement, and the measurement was cheap.

### The test that settles it

A translation unit is contiguous in the linked image, so if the optimisation
level is a per-unit property then ADJACENT functions must agree:

```
addr A     addr B      A          B          verdict
822D40F8   822D4118    /O2 only   /O2 only   AGREE
82540728   82540750    /O2 only   /O2 only   AGREE
825E3598   825E35C8    /Os only   /Os only   AGREE    48 bytes apart
82600BB0   82600BD8    /O2 only   /O2 only   AGREE
826FE5B8   826FE5C8    /O2 only   /O2 only   AGREE    16 bytes apart
827245C0   827245E0    /O2 only   /O2 only   AGREE

six informative adjacent pairs, six agreements, no split
```

Thirty of the sixty-four matched functions are flag-insensitive â€” they give
identical bytes at every level â€” and are excluded from that count, because a
pair that agrees for free is not evidence.

`src/manifest.txt` therefore carries a `flags=` column, and `build.py`,
`verify.py` and `objdiff_export.py` honour it. Only the optimisation flags go
in the column: `/c` and `/nologo` are prepended, because a row that forgets
`/c` makes `cl` try to LINK and report a diagnostic about the linker rather
than about the flags.

### And the reassuring half: TU context does NOT affect codegen

The obvious next worry is worse than the first: if a function's bytes depend
on what else is in its file, matching would require reconstructing whole
translation units before anything could be verified.

It does not. The same function was compiled six ways â€” alone; with a
companion after it; with a companion before it; with companions on both
sides; with a register-hungry seven-argument function before it; and with
that function after it â€” and produced **byte-identical code every time**.

```
alone                                  3d600000 394b0000 91430000 4e800020
one companion AFTER it                 3d600000 394b0000 91430000 4e800020
one companion BEFORE it                3d600000 394b0000 91430000 4e800020
companions both sides                  3d600000 394b0000 91430000 4e800020
a register-hungry function before it   3d600000 394b0000 91430000 4e800020
a register-hungry function after it    3d600000 394b0000 91430000 4e800020
```

Whatever decides register allocation is **inside the function**. That is why
the six remaining stalls cannot be explained by TU reconstruction, and it is
also why per-function matching is a sound unit of work.

---

## 7n. MSVC switch dispatch, and why Ghidra cannot follow it

*measured 2026-08-28*

Of 14,708 `bctr`/`bctrl` sites in `.text`, **104 are switch dispatches** and 95
are decoded. The rest are virtual calls. Two forms, and **neither is a table
of addresses** â€” which is exactly what Ghidra's `PowerPCAddressAnalyzer` looks
for, and why it mishandles them. There is no address anywhere: only an offset,
and a base built from a `lis`/`addi` pair.

**Byte form, 51 sites** â€” `caseBase + 4 * byteTable[value]`:

```
cmplwi rV, N                   the bound: N+1 cases
bgt-   default
lis    r12, hi(byteTable)      byteTable is in .rdata
addi   r12, r12, lo(byteTable)
lbzx   r0, r12, rV
rlwinm r0, r0, 2, 0, 29        * 4, so the byte is a WORD index
lis    r12, hi(caseBase)       caseBase is the word AFTER the bctr
addi   r12, r12, lo(caseBase)
add    r12, r12, r0
mtctr  r12 ; bctr
```

**Halfword form, 53 sites** â€” `caseBase + halfTable[value]`, for switches
whose bodies span more than 1 KB:

```
rlwinm r0, rV, 1, 0, 30        * 2 to index halfwords, so the LOAD's index
lhzx   r0, r12, r0             register is NOT the switch value
...                            no post-load scale: the halfword IS the offset
```

### Three bugs, each caught by a validation signal

**Reading a fixed 256 bytes of table invented case targets**, which showed up
as 7 collisions with `.pdata` function starts. `.pdata` is the compiler's own
table, so a collision there cannot be a discovery â€” it can only be the
extraction over-generating. The bound is the `cmplwi rV,N` before the guard.

**The bound compares the switch VALUE**, and in the halfword form the load's
index register is that value already scaled by two. Keying the search on the
load's register found nothing at all, 104 of 104.

**`rlwinm` is M-form**: its destination is `rA` and its source is `rS`, the
opposite way round from the D-form loads sitting beside it in the same
dispatch. Reading it as a load left all 53 halfword dispatches unbounded even
after the previous fix.

### The result is a negative, and that is the useful part

Case bodies are labels inside a function, not functions, and nothing reaches
them by `bl` or `b`, so `discover.py` cannot see them and correctly does not
list them. The question worth asking is the reverse â€” is anything ALREADY
listed as a function really a case body? Over 2,571 recovered targets the
answer is **none**, from `.pdata` or from discovery. `verify.py` checks it on
every run.

It does **not** explain the 13 functions Ghidra lists and discovery does not:
none of them is a case target. That remains open.

*(An earlier run reported 3 discovery entries sitting on case targets. They
were artifacts of the unbounded extraction and are gone; the claim was
premature and is not kept.)*

---

## 7o. Tooling added: objdiff, a permuter, and one runner

*2026-08-28*

**`tools/objdiff_export.py`** synthesizes what objdiff needs and this project
does not have. objdiff diffs two OBJECT FILES per unit; our target is a linked
image and our base is a COFF object, so both sides are emitted as PowerPC ELF
relocatables plus `objdiff.json`. **Verified end to end against objdiff-cli
3.8.0** rather than assumed: it reads them, decodes PowerPC, and reports 64
complete units.

The export includes the functions that do NOT match, from `src/attempts.txt`.
A unit list where every row reads 100% shows nothing. The base has its
relocations pre-resolved, as `build.py` does, or every `bl` would read as a
difference in a function that verifies perfectly.

The CLI found a defect immediately: the target object was being built at OUR
code's length, so where ours is the wrong size (`827FE808` compiled to 20
bytes against a 16-byte target) four bytes of the NEXT function were spliced
into the target and shown as a difference that is not there.

objdiff **cannot decode VMX128**. None of the matched functions contain any â€”
0 of 325 instructions, checked â€” but the engine's vector maths will not
render.

**`tools/permuter.py`** mutates a source in ways that cannot change what it
computes, compiles each with the real XDK compiler, and scores it. It
validates against a known answer: `sub_826C0FC8` is 2/6 as a free function and
6/6 as a member, and `--selftest` requires it to rediscover that. Three
defects, all found by validating rather than by running it:

* substituting a parameter name across raw text rewrote `the target's own` in
  a COMMENT to `the target'this own`;
* converting `sub_826C1480` to a member silently SHADOWED the member `f[12]`
  with its parameter `int f`;
* the scorer counted relocated words as mismatches, so a 4-of-6 function
  scored 1 of 6 and the hill-climb was guided mostly by noise.

**It has not cracked a single stall**, and that is the honest result. The
remaining six are register assignment, instruction order and branch
probability, and none of its seven mutations reaches register allocation.

**`tools/verify.py`** runs everything, including five negative controls, and
reads `src/manifest.txt` rather than keeping its own copy of the list â€” a
second copy is the drift that let three compile harnesses fall out of step
with `build.py`.

---

## 7l. LTCG SETTLED — the Rich header is a census, and it can be calibrated

*measured 2026-08-28*

This was the top open question in the project, because it decided whether the
matching loop could work at all. Under `/GL`, the compiler emits intermediate
language and `link.exe` does codegen, so the object holds **no PowerPC code to
compare against**. Directly measured here earlier:

```
without /GL :  626 B object, machine 01F2, 1 PowerPC function, 76 code bytes
with    /GL : 4737 B object, machine 0000, 0 PowerPC functions,  0 code bytes
```

The previous attempt tried to answer it by linking both ways and diffing the
linked output. That scored 0/19 for **both** arms, including a build known to
score 13/19 at object level, so the harness was wrong rather than the answer.

**The answer was already in the image, and had been read past twice.** §2
decodes the Rich header and records its rows, then says:

> The prodid -> tool NAMES are NOT_MEASURED. The community prodid table was
> not consulted; naming 132 "the C++ compiler" from memory is exactly the move
> that produces a confident wrong name.

That caution was right, and the conclusion drawn from it was wrong. Refusing to
*look up* the names was correct. Concluding that the names were therefore
unavailable was not — **the toolchain that stamped them is sitting in
`SDKFiles/`.** A meaning that can be measured does not need a table.

### What the Rich header is

Not a version banner. It is a **census**: one row per (product id, build), with
a count of how many objects that tool contributed. The per-object term of the
same sum is the absolute symbol `@comp.id`, whose value is
`(prodid << 16) | build`.

That gives two instruments over the same fact from different places — the
linker's tally in the DOS stub, and each compiler's own stamp inside each
object. They can disagree, so agreement means something.

* `tools/rich.py` decodes the header, refusing rather than guessing on any of
  five structural checks.
* `tools/compid.py` reads `@comp.id` out of objects and archives.
* `tools/rich_calibrate.py` builds known flag combinations and reports which
  ids appear.

### The product ids, MEASURED against this XDK

Each row below is a single-variable change from the control arm. The control
is one C++ TU with no `/GL`; it must produce prodid 132, the id that dominates
the retail image, or nothing else is interpretable and the harness refuses.

| prodid | meaning | how it was established |
|---|---|---|
| **131** | C, no `/GL` | adding `/TC` moved one object 132 -> 131 |
| **132** | C++, no `/GL` | the control arm |
| **137** | C, `/GL` | `/TC /GL` moved one object 131 -> 137 |
| **138** | C++, `/GL` | `/GL` moved one object 132 -> 138 |
| **145** | linker | exactly one per image, in every arm |
| **146** | export descriptor | disappears when `dllexport` is removed |
| **147** | XEX import descriptor | only ever on `xam.xex@...`, `xbdm.xex@...` |
| **149** | PowerPC assembler | see below |
| 1 / build 0 | import descriptors | 52 in a minimal link, 324 in retail |

**149 is the assembler.** Every one of the 89 objects in the XDK carrying it is
hand-written PowerPC: `crtgpr.obj`, `crtfpr.obj`, `crtvmx.obj`, `memcpyp.obj`,
`strlenp.obj`, `setjmp.obj`, `longjmp.obj`, `chkstk.obj`, `stackppc.obj`,
`sqrta.obj`, `fibera.obj`, `intrlock.obj`, `ppcbetramp.obj`. `crtgpr`/`crtfpr`
are `__savegprlr`/`__restgprlr` — already known to be assembly from §7e. The
`p` and `a` suffixes are that build's convention for PowerPC assembly sources.

**The id is stamped at COMPILE time, by `/GL`, not at link time by `/LTCG`.**
An arm that compiled `/GL` and linked *without* `/LTCG` still produced 138. So
the header records how each translation unit was compiled, which is exactly the
question.

### The retail image, read through that table

```
prodid  build  count   meaning
   131   8153    359   C,     no /GL
   132   8153   1090   C++,   no /GL
   149   8153     25   PowerPC assembly
   138   8153     54   C++, /GL          <-- the only link-time-codegen rows
   137      -      0   C, /GL            <-- ABSENT
   147   8153      5   XEX import descriptors
   145   8153      1   linker
   146   8153      1   export descriptor
     1      0    324   import descriptors
   123   2909      2   see the correction below
   109   2909      1   "
```

**Of 1,528 objects carrying a code-producing stamp, 1,474 were compiled
without `/GL` — 96.5%.** Fifty-four were not, and no C translation unit used
`/GL` at all.

**So object-level byte matching is sound methodology for this image.** Not by
inference: `sub_822607F0` is at an address inside the game's own band and it
matched 30/30 words as a compiled object (§7d). The game's own code demonstrably
compiles to comparable objects.

### The cross-check, from the other instrument

`tools/compid.py --join` reads `@comp.id` for every object `libmatch.py`
matched byte-for-byte into the retail image — 6,541 functions from **610
distinct objects across 27 libraries**:

```
prodid 132  x363   C++, no /GL
prodid 131  x230   C,   no /GL
prodid 149  x17    PowerPC assembly
prodid 137/138  x0
```

Not one carries a `/GL` id, which is what a byte match requires. Two
instruments with nothing in common agree.

*(Note: a `/GL` object carries no `@comp.id` this parser can read at all — the
IL container has machine 0000. So `--join` confirms the matched objects are
stamped plain C/C++/asm; it cannot separately detect a `/GL` object masquerading
as one. It does not need to: such an object holds no PowerPC code to match.)*

### Which library variants the retail build linked — ANSWERED

`XDK/lib/xbox/` ships `ltcg` variants beside ordinary ones, and which the
retail build used was recorded as NOT_MEASURED. It is measurable: an LTCG
library's members are IL with no `@comp.id`, and 2,595 such objects sit across
11 `*ltcg*.lib` files.

**Nine of the matched libraries have an LTCG twin, and in all nine cases the
NON-LTCG variant is the one that matched byte-for-byte:** `d3d9`, `xact`,
`xaudio`, `xavatar`, `xmahal`, `xmcore`, `xuirender`, `xuirun` (and `d3d9ltcgi`).
The two variants cannot both be linked — they define the same symbols — so the
LTCG twins were not linked.

### What the 54 are is still NOT_MEASURED

They are either the title's own `/GL` translation units, or members of an LTCG
library that has no matched twin. Only two such libraries exist —
`xact3ltcg.lib` (60 objects) and `x3daudioltcg.lib` (3) — and neither
`xact3.lib` nor `x3daudio.lib` appears among the matched libraries.

The image's extracted source paths contain no XACT3 or X3DAudio reference, but
**that negative is uninformative**: those 188 paths contain no `xact` or
`xaudio` reference of any kind, so the instrument cannot see audio libraries.
Checked before quoting, per the rule that produced it.

This does not gate anything. At worst it is 54 of 1,528 objects — 3.5% — and a
function that will not match for this reason would be found by failing to match.

### Correction to §2: the `2909` toolchain is NOT third-party

§2 records:

> The `2909` pair is a prebuilt third-party library — consistent with the
> `BINK`/`BINKCONS`/`BINKDATA`/`BINKBSS` sections, which RAD ships as objects.

**That is wrong.** A minimal test program built here — linking no middleware
whatsoever — reproduces both `123/2909` and `109/2909` in its own Rich header.
The census locates them exactly:

```
prodid 109  build 2909   xboxkrnl.lib   obj/xbox/bldnum.obj
prodid 123  build 2909   xboxkrnl.lib   xboxkrnl.exe@8276.0+1861.0   (x3)
```

They come from **`xboxkrnl.lib`**, an XDK library every title links. The Bink
reading was a plausible story fitted to a number, and the number had a duller
source. It also means the retail image contains **no evidence of a second
toolchain** — the earlier reading of "a second, older toolchain" in §2's table
is an artifact of the same mistake.

### Correction to §2: row order

§2's table lists `1 / 0 / 324` as the fourth row. In the file it is the sixth,
after the two `8153` rows for prodids 138 and 149. Row order is the linker's
own emission order, so it is data; the table was reordered by hand when written.

### PGO — the tooling now runs, the measurement does not

The competing hypothesis for the stalled matches was profile-guided
optimisation. `/LTCG:PGI` fails here with `LNK1123: failure during conversion
to COFF` on the `.pgd`, so which prodid a PGO build stamps is **NOT_MEASURED**
and the retail image cannot yet be checked for it.

What *is* established is that PGO is not needed to explain the stalls: the
branchless `sub_826C1480` cannot be affected by branch-probability data and
still will not match (§ MATCHED.md), and LTCG — the other candidate — is now
excluded for 96.5% of the image.

### R6034: fifteen XDK modules ship with no activation context

Found while running the PGO arm, which popped the same R6034 dialog that
`link.exe` produced before its manifest was repaired. `tools/pemanifest.py`
answers this statically, without running anything — running a broken tool pops
a **modal dialog that blocks until a human clicks OK**, which is a poor
instrument.

Of 160 modules in `XDK/bin/win32`:

```
139  do not import the VC90 CRT and cannot raise R6034
  6  carry an embedded VC90 manifest        link.exe (repaired), mspdb*, msobj*, msdis*
  1  covered by an external .manifest       cl.exe   <-- why cl works and lib.exe does not
  8  DLLs with no manifest                  c1, c1xx, c2, pgodb90, xpft90, ...
  6  EXEs with no manifest -- BROKEN        dumpbin, editbin, lib, pgocvt, pgodump, pgomgr
```

A first version of that tool reported cl.exe as "WILL RAISE R6034" while cl.exe
was demonstrably compiling. It was not distinguishing an EXE that can be covered
by an external `<name>.manifest` from a DLL that inherits its host's context.
**A guard that fires on known-good input teaches you to reach past guards**, so
it was fixed before being used.

`tools/fix_manifests.py --write` repairs the six by writing an external
`<name>.exe.manifest` beside each — the mechanism Microsoft already used for
`cl.exe` in that same folder. It modifies no binary and is undone by deleting a
file, unlike the `mt.exe` embedding that repaired `link.exe`. Verified:

```
dumpbin.exe    Microsoft (R) COFF/PE Dumper Version 9.00.8153
lib.exe        Microsoft (R) Library Manager Version 9.00.8153
pgomgr.exe     Microsoft (R) Profile Guided Optimization Manager 9.00.8153
```

### The rule this paid for

**A fact you declined to look up is not the same as a fact you cannot
measure.** The prodid names were marked NOT_MEASURED for a good reason — the
community table is for a different compiler generation and naming from memory
invents answers. But the tool that assigns those ids was already extracted in
this repository, and one afternoon of controlled builds gave the table
directly, with a control arm that refuses to interpret anything if it does not
reproduce the retail image's dominant id.

The cost of not noticing: three functions attempted, eight source shapes, 65
flag combinations, and a linked-output harness that scored 0/19 on a build
known to score 13/19 — all to answer a question the image had already answered.

---

## 7b. SCOPE REDUCTION — the real number (2026-08-28, corrected mapping)

Everything in §7 below was measured through the broken `.text` mapping and is
superseded. Re-run against `build/default.image.exe`:

```
DISTINCT IMAGE SITES IDENTIFIED   6541
  at a .pdata function start      5642        <- credible by construction
  inside a function                  9        <- was 6069 under the bad mapping
  in a gap                         890        <- leaf functions, no unwind row
```

**The "inside a function" class was almost entirely an artefact of the shifted
addresses.** It fell from 6,069 to 9. The elaborate boundary analysis built to
decide whether those were real was answering a question the mapping bug had
invented — which is worth remembering: a sound method applied to bad inputs
produces a sound-looking result about nothing.

| | functions | bytes | of `.text` |
|---|---|---|---|
| at a `.pdata` start, any symbol | **5,642** | 2,668,288 | **31.5%** |
| at a `.pdata` start, unique symbol | 4,710 | 2,523,892 | 29.8% |
| **REMAINING TO DECOMPILE** | **15,596** | **5,799,676** | **68.5%** |

By library: xgraphics 2,093, d3dx9 1,772, xaudio2 604, d3d9 361, xapilib 301,
libcMT 217, xaudio 114.

The matches are unambiguous — the whole `D3DXShader` compiler is statically
linked into the title and now identified by name:

```
824A60C0  38536 B  ?ImportExpression@Compiler@D3DXShader@@...
8247E840  30316 B  ?Simplify@Compiler@D3DXShader@@...
8248C5A0  27796 B  ?OptimizeLoops@Compiler@D3DXShader@@...
82519828  25448 B  ?IL2IR@CFG@XGRAPHICS@@...
8245E220  19460 B  ?Vectorize@Compiler@D3DXShader@@...
```

An independent corroboration: `823B1AD8` matches
`?Production@CParse@D3DXShader@@IAAXII@Z` at **9,464 bytes**, which is exactly
the `.pdata` extent recorded for that address by a different tool on a different
day.

*measured 2026-08-28, 62 non-LTCG release libraries, 133,379 library functions
examined, 85,618 indexable, 2,133,029 aligned positions scanned, `--min-bytes 32`*

---

## 7k. Ghidra, third run: names applied and the VMX gap partly closed

`ApplyKnowledge.java` runs before analysis and applies everything established:

```
imports:  207 row(s), 207 named, 207 function(s) created, 0 failed
rtti:    1296 row(s), 759 named, 537 had no function
prof:     254 row(s), 254 named
vmx gaps: 1407 short function(s), 24455 resume point(s) disassembled
```

**1,043 functions now carry a real name.** The 537 RTTI rows with no function
are the vtable slots that point at adjustor thunks rather than inventory
functions — already measured in §7g, not a new loss.

| | run 2 | run 3 |
|---|---|---|
| functions | 25,737 | **25,927** |
| instructions | 1,899,447 (89.7%) | **2,060,734 (97.3%)** |
| call edges | 73,653 | 73,685 |

**The VMX repair added 161,287 instructions — 7.6% of `.text`.**

### Two things it did NOT do, stated because the script's own counter lied

The script reported `body bytes 6855092 -> 6855092 (+0)`, and that counter is
**wrong**: Ghidra computes a function's body at creation time and does not
recompute it when later disassembly fills a hole. Function-body coverage is
therefore unchanged at 93.7% exact / 90.0% bytes, while the instructions
genuinely exist. This is the second time in this project that measuring a
repair by function-body extent produced a false `+0`; the honest instrument is
the whole-program instruction count from a separate script.

**Call edges gained only 32.** 161k instructions bought almost no new graph,
which is the expected shape — the recovered code is VMX128 arithmetic, and
arithmetic does not call anything. The gain is readable code, not connectivity.

The VMX128 words themselves remain undefined in Ghidra: it still cannot decode
them, and the repair only resumes disassembly *after* each one.
*measured 2026-08-28*

---

## 7j. Every kernel/XAM import is named — 207 of 207

The XEX header's import table (optional header `0x0103`, file offset `0x28E4`)
declares two libraries:

```
xam.xex        110 records
xboxkrnl.exe   316 records
```

Each import occupies two records — a variable slot in `.rdata` and a 16-byte
thunk in `.text` — and the dword at each holds `(type << 24) | ordinal`:

```
82000600  0000028B   type 0, ordinal 0x28B   (variable slot)
8291266C  0100028B   type 1, ordinal 0x28B   (thunk word 1)
82912670  0200028B   type 2                  (thunk word 2)
82912674  7D6903A6   mtctr r11
82912678  4E800420   bctr
```

**The names come from the XDK's own import libraries**, which carry short-import
records (`Sig1 == 0, Sig2 == 0xFFFF`) pairing an ordinal with a symbol.
`xboxkrnl.lib` identifies itself as `xboxkrnl.exe@8276.0` — the same XDK build
the title was compiled against, so both sides come from one source rather than
a table written from memory. 742 kernel + 390 xam ordinal records.

```
import records walked 426:  219 variable slots, 207 thunks
thunks: 207 named, 0 with no ordinal match
```

`XamUserGetSigninState`, `XamShowDirtyDiscErrorUI`, `XNotifyGetNext`,
`XamContentCreateEx`, `XMsgStartIORequest` … written to `build/imports.txt`.
*measured 2026-08-28*

---

## 7i4. VMX128 closed out — exhaustive table check, and an error in the documentation

The earlier checks covered 16 hand-transcribed forms and 33 the compiler
happened to emit. That left forms verified by nothing. `tools/vmx128_table.py`
parses **all 80 VX128 entries** out of binutils' source and checks three things
over the whole population.

**1. The table is unambiguous over the real image.**

```
distinct opcode-4/5/6 word(s) in .text  24,363
  matching exactly one VX128 entry      18,983
  matching NONE (plain VMX/Altivec)      5,380
  matching MORE THAN ONE                     0
```

A first version reported **348 ambiguous words** (`vupkhsb128` vs
`vupkhsh128`). That was my extractor reading `//{ "vupkhsh128", ... }` — lines
disabled in binutils' source — because the regex ran over the whole file
instead of skipping comments. The ambiguity was entirely an artefact of the
tool. Fixed; the real count is zero.

**2. The whole opcode-4/5/6 space decodes.**

```
opcode-4/5/6 instruction words   58,976
  a VMX128 form                  44,956  (76.2%)
  plain VMX/Altivec              14,020  (23.8%)
  UNDECODED                           0  (0.00%)
```

The plain forms present include `vmaddfp` (3,971) beside `vmaddfp128` (152),
and `vspltisw` (254) beside `vspltisw128` (244) — independent confirmation that
the `128` suffix is a register-allocation outcome and not a distinct operation.

**3. `vmx128.txt` HAS AN ERROR, and it is now identified.**

The document gives `vandc128` and `vnor128` the **same** encoding
(`|A|1 0 1 0|a|1|`), which cannot both be right. Across 32 forms compared,
that is the only disagreement with binutils — and Microsoft's encoder settles
it:

```
__vandc -> vandc128  143FF271   bits22-25 = 1001
__vnor  -> vnor128   143FF2B1   bits22-25 = 1010
__vand  -> vand128   143FF231   bits22-25 = 1000
__vor   -> vor128    143FF2F1   bits22-25 = 1011
```

**`vandc128` is 1001.** The document's 1010 duplicates `vnor128` and is used by
no encoder or decoder. This is a second defect in the reference material, after
the binutils operand-list bug in `vupkhsb128`/`vupklsb128` (§7i2) — which the
table dump also explains precisely: binutils declares those entries as
`{ VD128, VB128, VA128 }`, a three-operand form where the instruction has two.

### Verdict

Decoding is settled. Three sources, every form in the image checked, one error
found in each of the two reconstructions, and both resolved against the
compiler that built the game.

**Two things remain NOT_MEASURED and neither blocks moving on:**

- `vmaddcfp128` (175 sites) and `vpkd3d128` (94) have no declared intrinsic in
  either XDK header. Whether they are reachable from ordinary vector
  expressions is untested. It matters only when a function containing one is
  targeted for matching.
- Ghidra still cannot decode VMX128 (issue #2094, open since 2020). Our own
  tools can; Ghidra shows the words undefined. Only the SLEIGH fork route
  changes that, and it is untried.
*measured 2026-08-28*

---

## 7i3. Writing VMX128, not just reading it — and the "unreachable forms" were not

Reading VMX128 was solved by `ppcdis`. **Matching** needs the other direction:
given `vmsum3fp128` in the disassembly, what C++ compiles back to it?

`tools/vmx128_intrinsics.py` compiles every vector intrinsic the XDK headers
declare (142 of them) under enough register pressure to reach vr32..vr127,
reads the `/FAsc` listing, and attributes mnemonics by a **differential**
against a baseline with no intrinsic call.

```
lvx128     14693  <- __lvx          vpermwi128   1578  <- __vpermwi
stvx128     8273  <- __stvx         vsubfp128     725  <- __vsubfp
vmulfp128   5259  <- __vmulfp       vmsum4fp128   667  <- __vmsum4fp
vspltw128   2494  <- __vspltw       vperm128      614  <- __vperm
vor128      2023  <- __vor          vcsxwfp128    477  <- __vcfsx
vmsum3fp128 2008  <- __vmsum3fp     vsldoi128     422  <- __vsldoi
```

### Three wrong versions of this tool, and the third mattered

1. **No register pressure.** With three live vectors the compiler emits the
   PLAIN form (`vor`, vr0..vr31), never `vor128`. The tool reported "no
   intrinsic" for intrinsics that demonstrably have one.
2. **Filler excluded as noise.** The pressure body itself compiles to
   `lvx128`/`stvx128`, and excluding those made it impossible to ever credit
   `__lvx` with producing `lvx128` — reporting the two most common
   instructions in the image as unwritable in C++. Fixed by differential
   counting against a baseline.
3. **THE CONCLUSION WAS STILL WRONG.** Even after that, 14 forms and 7.3% of
   instructions read as "NOT reachable... would block any function using
   them". Testing those with correct signatures shows what actually happens:

```
__vmaddfp(s00,s01,s02)   ->  vmaddfp  vr1,vr0,vr13,vr12     <- PLAIN form
__lvlx((const void*)b,i) ->  lvlx     vr1,r6,r5
__vspltisw(3)            ->  vspltisw vr1,3
```

**The `128` suffix is a register-allocation outcome, not a different
intrinsic.** The same intrinsic emits the plain encoding when its operands
land in vr0..vr31 and the 128 encoding when they do not. So those forms were
never unreachable; my probe simply failed to force high registers for those
particular operations, and I nearly recorded a blocker that does not exist.

For matching this is the good news: you write the intrinsic and reproduce the
function's structure, and the allocator produces the encoding the original had.

**Two forms have no declared intrinsic at all**: `vmaddcfp128` (175 sites) and
`vpkd3d128` (94). `__vmaddcfp` and `__vpkd3d` are absent from both headers.
`vmaddcfp` is plain multiply-add with the destination as an operand, so it is
probably reachable from ordinary vector arithmetic rather than an intrinsic —
NOT_MEASURED. `__vupkd3d` and `__vrlimi` DO exist and were found by the
targeted probe (`vupkd3d128`, `vrlimi128`).
*measured 2026-08-28*

---

## 7i2. The decisive VMX128 oracle: Microsoft's own encoder

Both earlier oracles were third-party reconstructions. Biallas's own preamble
says *"I figured this out by looking at various disassemblies, so there might
be some errors"*, and Ghidra issue #2094 records a later worker finding errors
in it. binutils' table has the same provenance problem.

**The XDK ships the authoritative encoder.** `cl.exe /FAsc` emits a listing
carrying both the encoded word and the register names the compiler chose:

```
0001c  17bef8b5   vmulfp128    vr61,vr62,vr63
0005c  169be1b5   vmsum3fp128  vr52,vr59,vr60
```

That is the compiler that built the retail image, stating what it encoded and
which of the 128 registers it meant.

`ml.exe`, the XDK's PowerPC assembler (also 8153), is **not** usable for this:
it defines only `vr0..vr31` and rejects the `*128` arithmetic mnemonics
outright, so it predates the extension. Checked, not assumed.

`tools/vmx128_oracle.py` generates a probe with enough live vectors to force
the allocator past `vr31`, compiles it, parses the listing and compares:

```
VMX128 instructions emitted      138
distinct vector registers         64   (32 of them above vr31)
  AGREE, mnemonic AND registers  136
  operands differ                  2
  mnemonic differs                 0
```

**The registers above vr31 are the load-bearing part.** A 7-bit register number
is scattered (`VD = VDh:VD128`, `VA = A:a:VA128`, `VB = VBh:VB128`); with only
low registers every one of those extra bits is zero and the check would pass
vacuously.

### A defect found in binutils, using MSVC as the oracle

The 2 disagreements are real and they are binutils' fault:

```
1A607B85   MSVC:     vupkhsb128 vr51,vr47
           Biallas:  vupkhsb128 vr(VD128), vr(VB128)      -- two operands
           binutils: vupkhsb128 v51,v47,v0                -- three
```

Decoding by the documented layout: `VD128=19, VDh=1 -> vr51`;
`VB128=15, VBh=1 -> vr47`; **bits 11..15 = 0**, which the document specifies as
a reserved zero field. binutils prints that reserved field as a third register
operand. The registers agree exactly and the instruction word is read
identically, so this is a FORMATTING bug in the disassembler, not a decode
error — and it affects `vupkhsb128` and `vupklsb128`.

Three sources now: MSVC's encoder, Biallas's tables, binutils' opcode table.
Two agree against the third on exactly one point, and it is cosmetic.
*measured 2026-08-28*

---

## 7i. The VMX128 decoder, marked against an independent source

`vmx128.txt` (Biallas, 2006) documents the encoding bit by bit. It and
binutils' opcode table are two derivations with nothing in common — one a
compiled opcode/mask list, the other hand-documented from dumpbin output.

`tools/vmx128_check.py` transcribes 16 of the forms straight from the document,
reassembles the split register fields the way the document specifies
(`VD = (VDh << 5) | VD128`, `VA = (A << 6) | (a << 5) | VA128`), and compares
against `ppcdis`:

```
24,363 distinct opcode-4/5/6 word(s) in .text
  covered by the 16 transcribed forms   14,617
  AGREE on mnemonic AND operands        14,617
  differ                                     0
```

Operands, not just mnemonics — the reassembled 7-bit register numbers are the
part a hand transcription is most likely to get wrong, and a mnemonic-only
check would not catch it.

**The oracle is known to be imperfect and that is stated rather than glossed:**
Ghidra issue #2094 records a later worker finding errors in `vmx128.txt` while
writing a SLEIGH implementation. So a disagreement would have been a question,
not a verdict. There were none.

### Ghidra still cannot decode VMX128, and that is not fixed here

Issue #2094 has been open since 2020; there is no support in mainline. Forks
exist (pjsoberoi, and 0dinD's rebase onto Ghidra 12.0 with corrections to both
the SLEIGH and the documentation), and are untried here.

`ApplyKnowledge.java` does NOT fix the decode. It resumes disassembly at the
word AFTER each undecodable one — sound because VMX128 instructions are all 4
bytes and none alters control flow — so surrounding code and the call graph
recover while the VMX128 words themselves stay undefined in Ghidra.

### MSVC switch tables: real, but smaller here than reported

A second Ghidra issue reports `PowerPCAddressAnalyzer` failing on MSVC's
PowerPC switch pattern (16-bit offsets via `lhzx`, tables in `.rdata` more than
4096 bytes from the load). Measured over this image rather than assumed:

```
bctr  (indirect jump)   1219
  no switch pattern      604  (49.5%)   tail calls / dispatch
  lwzx 32-bit table      347  (28.5%)
  bounded, no table      211  (17.3%)
  lhzx 16-bit table       57  ( 4.7%)   <- the specific pattern reported
bctrl (indirect call)  13541
```

So the reported bug can affect at most **57 sites**, not the bulk of them. The
larger unresolved population is the 347 `lwzx` switches and the 604 with no
switch shape at all.
*measured 2026-08-28*

---

## 7h. VMX128 is readable — 44,956 instructions that no tool here could decode

Capstone has no Xenon VMX128, and neither does Ghidra's
`PowerPC:BE:64:A2ALT-32addr`. That left 1,330 functions truncated in Ghidra and
every VMX128 word printed as `.long` everywhere else — concentrated in exactly
the vector maths a game engine is full of.

**Not reimplemented — the real opcode table is used.** binutils' PowerPC
disassembler carries the extension, gated behind `PPC_OPCODE_VMX_128`, which
the `"cell"` dialect option sets. `thirdparty/disasm/` holds a copy of
`ppc-dis.c` and friends (binutils-derived, from XenonRecomp's vendored copy)
plus `ppcdis_main.c`, built to `build/ppcdis.exe` with MSVC.

Transcribing the encoding by hand was rejected deliberately: the VX128 forms
use **six different field masks** (`0x3d0`, `0x7f3`, `0x210`, `0x7f0`, `0x730`,
`0x10`), and a wrong transcription decodes into something plausible.

**Validated against known-good answers before use.** On `sub_822607F0` — the
function already matched byte-for-byte — it agrees with capstone instruction
for instruction (the sole difference being `slwi r11,r10,1` against
`rlwinm r11,r10,1,0,30`, the same instruction in extended and raw mnemonics).
On a VMX-dense function it decodes what capstone cannot:

```
827A7EC8  108028C3   capstone: .long        binutils: lvx128 v4,r0,r5
```

**Coverage over all 2,116,991 words of `.text`:**

```
undecoded         16,673   0.788%   (data interleaved in code)
VMX128 decoded    44,956   2.12%    in 55 distinct forms
```

```
lvx128 14693   stvx128 8273   vmulfp128 5259   vspltw128 2494
vor128  2023   vmsum3fp128 2008   vpermwi128 1578   lvlx128 935
```

`vmsum3fp128` is a three-component dot product; its 2,008 sites are geometry
and physics.

`tools/ppcdis.py` wraps it, and `tools/disasm.py` and `tools/match.py` both use
it now — **one decoder for both sides of a diff**, since a comparison where one
side reads `.long` is not a comparison of instructions. The capstone fallback
prints a warning rather than degrading silently.
*measured 2026-08-28*

---

## 7g. Havok without the Havok SDK, and a corrected denominator

**The inventory was ~18% short and every percentage quoted before this was
computed against it.** `.pdata` is the compiler's unwind table and omits leaf
functions needing no unwind row. `tools/inventory.py` unions it with Ghidra's
analysis:

```
.pdata functions          21,238
ghidra functions          25,737
union                     25,737     (0 in .pdata that Ghidra lacks)
  size from .pdata        21,238
  size from Ghidra only    4,499
bytes covered           8,254,979    = 97.5% of .text
```

### RTTI gives Havok's classes and vtables, with no SDK

Havok was built with RTTI on; the game's own code was not (`/GR-`), so all
**329 type descriptors are Havok** — 322 `hk*` plus 7 in Havok's anonymous
namespaces. Walking MSVC's chain backwards then forwards
(`TypeDescriptor` <- `CompleteObjectLocator` <- `vtable[-1]`):

```
type descriptors          329
  with a locator          270      (46 had none)
  locators                350
  vtables                 311
vtable slots read       2,873
  target is a .pdata fn  1,423     (49.5%)
distinct functions      1,296
  in exactly one class   1,021
```

**This is the answer to not having the Havok libraries.** A byte-matching
decomp does not have to reconstruct middleware — it has to *identify* it, so it
can be excluded from scope. RTTI identifies it directly.

**Two wrong turns while tightening the walk, both recorded because the second
undid the first.** The terminator was "while the slot points into executable
memory", which I feared ran off one vtable into the next. Requiring each slot
to name a known function instead collapsed `hkpMotion` from 26 slots to 4 —
vtables legitimately hold adjustor thunks that are not inventory functions.
Adding "stop where the next vtable begins" then reported **0** such stops: the
`CompleteObjectLocator` pointer sits immediately before every vtable and is not
a code address, so the original rule already terminated correctly. The fear was
unfounded and the first rule was right.

### Scope, on the corrected denominator

```
  signal               fns        bytes  share of .text
  lib                 6332      2759109   32.6%
  srcpath              352       241004    2.8%
  havok                251       157868    1.9%
  rtti_havok           673       168776    2.0%
  game_profiled          3         8320    0.1%
  TOTAL known         7611      3335077   39.4%
  REMAINING          18126      5132887   60.6%
```
*measured 2026-08-28*

---

## 7f. Analysis, corrected — and the residual is fully explained

Fresh import with `Non-Returning Functions - Discovered` disabled *before*
analysis, `.pdata` applied as a preScript so analysis starts from the compiler's
own function table.

| | broken run | corrected run |
|---|---|---|
| functions | 20,795 | **25,737** |
| instructions | 773,768 | **1,899,447** (89.7% of `.text`) |
| call edges | 24,835 | **73,653** |
| with >=1 caller | 6,267 (30%) | **18,028 (70%)** |
| leaves | 3,085 | 4,782 |
| wall time | 357 s | **216 s** |

Verified per function against `.pdata` rather than taken on trust:

```
21,238 of 21,238 present
  body EXACTLY the .pdata size  19,908  (93.7%)   was 8,454
  body SHORTER                   1,330            was 9,647
  byte coverage                   90.0%           was 20.7%
  median coverage ratio            1.00           was 0.11
```

**The residual 1,330 is VMX128, and this time the measurement supports it:
1,156 of the 1,330 (86.9%) contain an opcode-4/5/6 word** — exactly the 1,156
functions independently counted as containing one. Ghidra's
`PowerPC:BE:64:A2ALT-32addr` does not decode Xenon's VMX128 extension, so those
bodies stop early; 585 stop at the first such word exactly.

The same hypothesis was *refuted* against the 60% gap and *confirmed* against
the 10% one. It was never wrong about VMX128 — it was wrong about which
population it explained.

### Candidate selection, corrected twice

**Ghidra's "leaf" means "no RESOLVED callee", not "makes no calls".**
`sub_827FE628` is listed as a leaf and makes two `bctrl` virtual calls. Leaves
must therefore be required by BOTH the call graph and a static check that no
instruction sets the link register (`bl`, `bcl`, `bctrl`, `blrl`).

With that, plus "not attributed", "outside the XDK bands", "no VMX128", and
"has at least one caller": **82 true leaves.** The already-matched
`sub_822607F0` appears in the list, which is the check that the filter is not
excluding valid targets.

```
  address     bytes  callers  floats
  82154AE8      536       42      66     <- heavily used float routine
  82700B30      176        9       0
  825FA9E8      176        8       0
  827618E8      136        3       0
  8215E820      104        2       0
  822607F0      120        2       0     <- already matched
```
*measured 2026-08-28*

---

## 7e. `__savegprlr`: one wrong no-return flag cost 60% of the disassembly

Ghidra's first full analysis reported figures that could not be right:

```
functions      20795     (21,238 were created -- it LOST 443)
instructions  773768     (.text holds 2,116,991 words -- 36.5%)
no caller      14528 of 20795   (70%)
```

Per-function, against the `.pdata` extents:

```
body EXACTLY the .pdata size   8454
body SHORTER                   9647
body LONGER                       0
median coverage ratio          0.11
worst: sub_824A60C0, .pdata 38536 bytes, Ghidra body 8 bytes
```

Eight bytes is two instructions. Those two are:

```
824A60C0  mflr r12
824A60C4  bl 0x828A7590
```

**`828A7590..828A75DC` is MSVC's `__savegprlr` — one routine with sixteen entry
points**, one per register count, all falling through to a single `blr`.
`828A97C0..` is the matching `__restgprlr`. **10,856 of 21,238 functions begin
`mflr r12 ; bl 828A75xx`**, each calling a *different offset into the same
body*, so 10,856 calls land mid-function.

`Non-Returning Functions - Discovered` concluded those entry points do not
return — **25 of them flagged**, every one of `828A7590`, `828A7594`, …,
`828A75CC` plus the restore helpers — and truncated every caller at its second
instruction.

**A first repair attempt failed, and the failure is worth recording.** Clearing
the flag and calling `disassemble()` on each truncated function moved the total
by **+0 bytes**: a function body is computed by flow analysis at creation time,
and `disassemble()` at an address that already holds an instruction is a no-op.
Clearing a flag does not retroactively recompute anything. The fix has to be a
fresh import with the analyzer disabled *before* analysis runs.

The VMX128 explanation was tried first and **refuted by measurement**: only
5.4% of functions contain an opcode-4/5/6 word, and truncating at the first one
would still leave ~96% coverage, not 39%.

**This matters beyond Ghidra.** Any matched function large enough to save
registers will call `__savegprlr`/`__restgprlr`, so the emitted prologue must
target the correct entry offset. `sub_822607F0` was small enough to avoid them.
*measured 2026-08-28*

---

## 7d. FIRST BYTE MATCH — `sub_822607F0`, 2026-08-28

```
target  822607F0  120 byte(s)
ours    ?BuildGridStripIndices@@YAXHHHHPAG@Z  120 byte(s)
30 word(s) compared: 30 identical, 0 differ
```

Reproduces from a clean build directory. Recorded in `MATCHED.md`;
re-checkable with `python tools/match.py src/grid_indices.cpp 822607F0`.

Flags `/O2 /Gy /GS- /fp:fast`. It generates 16-bit indices for a grid drawn as
one triangle strip, with degenerate indices stitching successive rows.

**How the target was chosen, which is the transferable part.** A first match
must be a LEAF — a function calling nothing — or a mismatch cannot be
attributed to the source you wrote rather than to a callee that does not exist
yet. `tools/candidates.py` finds leaves statically (no instruction sets the
link register) among unattributed functions: **199 of 14,992 are leaves, 1.3%**,
of which 147 are 400 bytes or under.

**A label in that tool was wrong and is corrected:** "unattributed" is not the
same as "the title's own code". Several of the smallest candidates
(`823054D4`, `82344774`, `823FB7A0`) sit inside the XDK block at
`822F03E8..82523A1C` — they are XDK functions that merely failed to byte-match.
Candidates must be filtered against the attributed regions in §7c.

**Three attempts, and the second was worse than the first.** Attempt 1 got the
instruction sequence structurally right at 124 bytes against 120. Attempt 2
introduced a `clrlwi` — a `u16` truncation materialised because the increment
and the store competed for one variable — and dropped to 0 of 30 words. Attempt
3 read the target's own register discipline out of the disassembly: it computes
`base+1` into its own register *before* either store, uses it for the second,
then adopts it as the new base and compares *that* against the limit. Writing
that shape explicitly produced the exact match.

The lesson is that the register allocation IS the specification. Guessing at
plausible C and hoping is slower than reading which value the target keeps live
and for how long.

---

## 7c. Consolidated attribution — what actually has to be decompiled

`tools/attribute.py` merges four independent signals. A function is counted
once, and the evidence classes are kept separate rather than summed into one
confident figure.

```
  signal               fns        bytes  share of .text
  lib                 5642      2668288   31.5%    byte-for-byte XDK match
  srcpath              350       240848    2.8%    references a middleware source path
  havok                251       157868    1.9%    pushes a Havok timer name
  game_profiled          3         8320    0.1%    pushes a Ttz name (the title's own)
  TOTAL known         6246      3075324   36.3%
  REMAINING          14992      5392640   63.7%
```

`lib` is a byte match and is strong. `srcpath` and `havok` are attribution by a
reference the function *makes*, which is weaker — a game function could log a
middleware path — so they are reported separately and never merged into the
byte-match figure.

Middleware by family: fmod 251, ogg_vorbis 50, havok 38, sfx 10.

### The linker grouped it, so the map is contiguous

```
  82100000..821294A0    169 KB   185 fn  lib     <- the D3D/XTL band
  822F03E8..82523A1C   2.31 MB  3972 fn  lib     <- one huge XDK block
  8273AEE8..827528D4     97 KB    72 fn  havok
  827B92F0..827BFF14     28 KB    25 fn  havok
  828A74A0..82908510    397 KB   922 fn  lib     <- the CRT
```

So the title's own engine lives mainly in the gaps — roughly
`82129000..822F0000` and `82523A1C..828A74A0` — and that is where decompilation
effort belongs. The `82100000..821294A0` band agrees with the D3D entry-point
addresses recorded independently elsewhere, which is a check on the whole
attribution rather than on one match.

### Profiler names, tightened

The first extraction produced names like `144 Et` by latching onto any nearby
`lis`/`addi` pair. It now requires the full chain — `mftb rT`, then
`stw rT, D(rBuf)` with the same register, then `stw rN, (D-4)(rBuf)` with the
same buffer, then the pair whose *destination* is `rN` — and refuses to report
unless it reproduces a name read by hand from `sub_826731D8` ("TtrcSphere").

```
717 mftb site(s): 579 named (80.8%), 122 no address pair,
                  13 no timestamp store, 3 no name store,
                  0 resolved to something that was not a string
224 distinct names, ALL 224 non-numeric   (was 326 with noise)
```

The names are Havok monitor-stream markers — `Tt` 138, `St` 56, `Lt` 27, `Et` 2.
Exactly **three** are the title's own, by the `Ttz` prefix:

```
821D3D48  3716 B  TtzCam2Player_update
82241BF0  2228 B  TtzNPCSteering_ApplySteering_hover
822CA548  2376 B  TtzSceneUpdate_CheckingTransparent
```

`sub_821D3D48` reads as a camera update: `this` in r3, a float delta in f1,
fields at `+0x16C`, `+0x174`, `+0x1EC`, `+0x2C8`.
*measured 2026-08-28*

---

## 7. Library matching — SUPERSEDED (measured through the broken mapping)

`tools/libmatch.py` matches XDK library code against the image so that code
which links from the original `.lib` need not be decompiled at all.

**Two bugs, each of which produced exactly 0 matches** — the failure shape this
project cares most about, because an empty result reads like a fact:

1. *Relocations.* Library object code carries placeholder bytes the linker
   patches. Every instruction word a relocation touches is masked and the match
   judged on the rest, with the compared byte count reported per match.
2. *`.pdata` was the wrong search space.* It covers 92.5% of executable bytes
   and what it omits is largely leaf functions needing no unwind data — most of
   the CRT. Indexing candidates on `.pdata` starts found 0 while a whole-image
   masked scan found `strncmp`, `longjmp` and `memchr` immediately.

Validated against known-good answers before use: **5 of 6** functions located
by an independent whole-image scan reproduce with the correct symbol name.

**Run over 62 non-LTCG release libraries**, 133,379 library functions examined,
85,618 indexable, 2,133,120 aligned positions scanned:

```
raw sites identified   6540
  at a .pdata start     104
  inside a .pdata row  6069
  in a gap              367
```

**FIRM LOWER BOUND: 399 functions, 171,972 bytes, 2.03% of `.text`** — the
sites at a function start or in a gap, whose library symbol matches exactly one
place. These are unambiguous and large: `EmitExpression@CCompiler@D3DXShader`
(6,912 unmasked bytes), `jpeg_fdct_float@D3DX`, `png_write_find_filter@D3DX`,
`BltTriangle3D@CBlt@XG_D3DXTex`, `MultiByteToWideChar`, `SetFilePointerEx`.
By library: xgraphics 132, xaudio2 107, d3dx9 59, d3d9 29, xapilib 22, libcMT 20.

**THE UPPER BOUND IS NOT MEASURED, AND MY FIRST FILTER WAS WRONG.** I discarded
all 6,069 "inside a `.pdata` row" matches as false positives, on the reasoning
that a match landing mid-function must be coincidence. Checking what PRECEDES
each one refutes that:

```
after nop/zero padding   3104  (51.1%)
after blr                1261  (20.8%)
mid-stream               1704  (28.1%)
```

**72% sit at an apparent function boundary.** Candidate readings, none of them
established: a `.pdata` row can cover more than one function; or these are
library bodies INLINED into callers; or they are coincidences that happen to
follow padding. Until that is settled the honest scope reduction is
"at least 2%, plausibly much more", and the 399 is the only number to quote.

*measured 2026-08-28; the 6,540 raw sites from one run at `--min-bytes 32`,
the precedence census over all 6,069*

---

## 8. THE .text MAPPING WAS WRONG, AND MOST RESULTS ABOVE ARE INVALIDATED

*found 2026-08-28, while closing an unrelated gap*

**The unpacked XEX is a MEMORY IMAGE: RVA == offset in the buffer.** The PE
headers still carry `PointerToRawData` from the original file layout, and every
tool written here believed it. For `.text` the two differ by `0xDE00`, so every
byte read from `.text` onward was the wrong byte.

The first three sections have `RVA == PointerToRawData`, which is why nothing
looked wrong until `.text`.

**It was settled by a control, not by whether the output looked like code** —
which matters, because the wrong mapping disassembles cleanly:

```
822F8BC8 via PointerToRawData:  lfs f13,0x2df0(r8) / stw r10,0xbc(r1) / lfs ...
822F8BC8 via RVA == offset:     mflr r12 / bl 828a75c8 / addi r31,r1,-0x1f0 / stwu
```

The entry point must begin with a prologue. Only one mapping delivers that.

### The verification that could not fail

Section 6 records "14 of 14 initialised blocks agree, at the right addresses"
and calls the Ghidra memory map sound. **That check was vacuous.** Ghidra's PE
loader maps by `PointerToRawData`; `tools/verify_ghidra.py` read the file by
`PointerToRawData`. The two agreed because they shared the same defect. Two
derivations of one fact with nothing forcing them to be *independent* is not a
cross-check — it is one derivation, run twice.

The catch is that it was still worth running: it did find two real checker bugs.
What it could never find was a defect common to both sides.

### Invalidated

- **`libmatch.py`, all 6,540 sites including the 399 "firm" ones.** The bytes
  compared were real image bytes, so the matches are probably real code — but at
  addresses reported `0xDE00` too high, which is also why so many appeared to
  land "inside" a `.pdata` function. The whole run must be redone.
- **`xref.py`, including the Time.cpp result.** "0 references to 82065B68" is
  NOT_MEASURED, not a finding. `Time.cpp` may yet be locatable.
- **Ghidra's program**, for `.text` and every later section.
- **The 562 disassembly failures.** 85 of them read as zero words only because
  the mapping pointed at unmapped space. The VMX128 classification (464 of 562,
  82.6%) is drawn from bytes that were the wrong bytes.
- **`strings.py` addresses at or beyond `.text`.** The classification counts hold
  and the source paths are in `.rdata`, where the mapping coincides, so §3 is
  unaffected.

### NOT invalidated

- The XEX unpacking (§1) — verified against page descriptors and block sums,
  neither of which involves the section table.
- The toolchain identification (§2) — the Rich header lives in the DOS stub,
  where RVA == raw.
- **The `.pdata` inventory (§4), 21,238 functions with sizes** — `.pdata` is one
  of the three sections whose mapping coincides, and the decode was marked
  against four arms. This is the one substantial result that survives.
- The source tree (§3) and middleware list.

### The fix

`tools/flatten_pe.py` rewrites `PointerToRawData := VirtualAddress` (and grows
`SizeOfRawData` to cover `VirtualSize`) into `build/default.image.exe`, so the
headers describe the file as it actually is. It verifies that **0 bytes outside
the section table changed** and that the entry point disassembles as a prologue.
`tools/peimage.py` now maps by RVA and documents why.

**`build/default.image.exe` is the file everything should use from now on.**

### Confirmed by a control that shares nothing with either mapping

`tools/verify_mapping.py` scores both mappings against the `.pdata` extents,
which take no part in choosing a mapping. Under a correct mapping a function's
last word must be a terminator at exactly the byte `.pdata` says it ends.

```
                     prologue    terminator    BOTH     first word zero
PointerToRawData        72.2%          4.8%     3.6%          85
RVA == offset           98.6%         99.6%    98.2%           0
```

**The terminator test is the load-bearing half.** The prologue test alone barely
discriminates (72% vs 99%) because a wrong mapping still yields decodable
instructions. Placing a return on the exact final byte of 21,150 of 21,238
functions is what a wrong mapping cannot do. It also independently confirms the
`.pdata` size decode.

### Re-run results

Ghidra, re-imported from the corrected file:

```
rows read           21238
  created           21238
  disassembly failed    0
  createFunction failed 0
program function count now 21238
```

**All 562 "disassembly failures" were the mapping.** There were none. The VMX128
classification recorded earlier was drawn from the wrong bytes and is withdrawn.

The earlier Ghidra reconciliation still holds and is now moot: 20,676 + 258
`FUN_` functions Ghidra found by following flow = 20,934, exactly the old count.
The 79 logged `CreateFunctionCmd` errors were at non-`.pdata` addresses, so they
came from Ghidra's own flow-following and not from `ApplyPdata` — the script's
`createFunction failed 0` was correct.

---

## 9. What the image says about its own source

`tools/srcfiles.py` resolves every `lis`+`addi`/`ori` pair landing on a source
path: **188 distinct paths referenced by code**, of which **11 are the title's
own** and 177 are middleware (FMOD, ogg/vorbis, Havok).

The `__FILE__` strings are **not** used by asserts inside those files. Each is
stored by a static initializer that registers its translation unit into a global
list — for `Time.cpp` that is at `82908BA0`, writing the string into a record and
bumping a counter at `[82A3521C]`. So the string does not locate the file's code,
which is why an xref for it returns one distant hit. That conclusion survives the
mapping fix: re-run correctly, `82065B68` still has **0 data references and 1
code reference**, and the code reference is the registration.

`tools/profnames.py` harvests the engine's profiler scope names: around each
`mftb` the code stores a literal name and a timestamp into a per-thread buffer at
`[r13]+0x30`. **717 sites, 540 carrying a readable name, 326 distinct names over
216 functions.**

Most are **Havok's** (`TtNarrowPhase`, `StBroadphase`, `TtGJK Ray Cast`,
`hkpShapeCollection::`), so they label middleware. A few are the game's own,
distinguished by a `Ttz` prefix: `TtzCam2Player_update`,
`TtzNPCSteering_ApplyStee`, `TtzSceneUpdate_CheckingT`.

**The name list contains noise and is NOT yet trustworthy.** Entries like
`144 Et` show the look-back latching onto the wrong `lis`/`addi` pair and
resolving to an arbitrary printable address. The extraction needs to require the
pair that actually feeds the `stw` beside the `mftb` before any of these names is
used to label a function.
*measured 2026-08-28*

---

## 9. Open, and what is not known

- **Two Ghidra numbers do not reconcile and this is unresolved.** `ApplyPdata`
  created 20,674 and found 2 existing, which is 20,676; the program then
  reported a function count of **20,934**. The 258 difference is unexplained.
  Separately, 79 `CreateFunctionCmd` errors were logged ("body contains
  referring thunk") while the script's own `createFunction failed` counter read
  **0** — so either the errors came from `disassemble()` following flow, or the
  counter is measuring the wrong thing. `DumpFunctions.java` is written and NOT
  YET RUN; it exports the whole set so the two populations can be diffed
  instead of two counts being compared.
  **Nothing should be concluded from the 20,934 until that diff is done.**
- **The 562 disassembly failures are UNCLASSIFIED.** The leading hypothesis is
  Xenon VMX128, which is not standard Altivec and which Ghidra's A2ALT variant
  may not decode; 562 of 21,238 is 2.6%, which is suggestively close to the
  VMX128 density of this image, but that number came from a different project
  and has not been re-derived here. Not measured: what the first word at those
  addresses actually is. One histogram of primary opcodes settles it.
- **`link.exe` fails to start** — see §5. Not blocking.
- **prodid → tool names** — NOT_MEASURED, see §2.
- **Which lib variant (ltcg or not) the retail build linked** — NOT_MEASURED.
- **The `2909` toolchain in the Rich header** — NOT_MEASURED; not from this XDK.
- **The source tree beyond 11 files** — NOT_MEASURED, see §3.
- No function has been matched yet. The loop is proven on synthetic code only.
