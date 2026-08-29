# Matched functions

Byte-for-byte matches against the retail image, compiled with the original
XDK 8276 toolchain (`cl.exe` 15.00.8153), every one at
`/O2 /Gy /GS- /fp:fast`.

**64 functions, 1508 bytes.** Verify all of them, plus the reconstructing
build and five negative controls, with one command:

```bash
python tools/verify.py
```

Every match is also a row in `src/manifest.txt`, so `tools/build.py` compiles
it, resolves its relocations against the retail bytes and splices it into
`.text`. Nothing here is a match on `match.py`'s word-comparison alone.

**The retail build did NOT use one optimisation level everywhere.** 8 of
these need `/O2 /Os`; the rest need plain `/O2`. See "Flags are a property of
the translation unit" below -- this was claimed the other way round for a
while and the claim was wrong.

| address | bytes | callers | source | symbol | flags |
|---|---|---|---|---|---|
| `82807B38` | 20 | 314 | `guard_tailcall.cpp` | - | `/O2` |
| `82600BD8` | 16 | 136 | `global_field.cpp` | - | `/O2` |
| `821A4628` | 28 | 108 | `ctor_vt.cpp` | - | `/O2` |
| `8253FD70` | 28 | 82 | `array_add.cpp` | - | `/O2` |
| `822D2450` | 24 | 63 | `table_index.cpp` | - | `/O2` |
| `82540750` | 28 | 49 | `string_utils.cpp` | StrCopy | `/O2` |
| `827C5198` | 20 | 48 | `vcall116.cpp` | - | `/O2 /Os` |
| `82540728` | 36 | 37 | `string_utils.cpp` | StrLen | `/O2` |
| `827007E8` | 16 | 32 | `set_vtable_827007E8.cpp` | - | `/O2 /Os` |
| `82677028` | 20 | 27 | `owner_clear.cpp` | ClearAndHandle | `/O2` |
| `82677040` | 20 | 27 | `owner_clear.cpp` | ClearAndHandleOther | `/O2` |
| `821636A8` | 24 | 26 | `chain5.cpp` | - | `/O2` |
| `826A3350` | 20 | 19 | `null_tailcall.cpp` | - | `/O2` |
| `82600BB0` | 20 | 19 | `vcall_arg2.cpp` | - | `/O2` |
| `826C5E00` | 28 | 19 | `vcall_global_arg.cpp` | - | `/O2` |
| `8224E7C0` | 16 | 17 | `arr_index0.cpp` | - | `/O2` |
| `821A4FA0` | 16 | 17 | `fwd_global.cpp` | - | `/O2` |
| `8224E080` | 20 | 16 | `vcall_f8_40.cpp` | - | `/O2` |
| `828864E0` | 20 | 16 | `vcall_arg_adj.cpp` | - | `/O2 /Os` |
| `822021F8` | 16 | 14 | `stride116.cpp` | - | `/O2` |
| `82639C28` | 16 | 11 | `chain_add48.cpp` | - | `/O2` |
| `826C0FC8` | 24 | 11 | `stride24.cpp` | - | `/O2` |
| `82166FD0` | 16 | 11 | `fwd_vec3.cpp` | - | `/O2` |
| `822020B0` | 16 | 10 | `chain2_156.cpp` | - | `/O2` |
| `821A4FB0` | 20 | 9 | `fwd_global_n.cpp` | - | `/O2` |
| `827245E0` | 32 | 9 | `ring_index2.cpp` | - | `/O2` |
| `8253FE28` | 28 | 9 | `zero48.cpp` | - | `/O2` |
| `821A5378` | 20 | 8 | `eq2_208.cpp` | - | `/O2` |
| `82603948` | 20 | 8 | `null_call0.cpp` | - | `/O2` |
| `82727258` | 16 | 7 | `stride8.cpp` | - | `/O2` |
| `8225FDD8` | 20 | 7 | `zero3.cpp` | - | `/O2` |
| `82265D30` | 20 | 7 | `set0_255.cpp` | - | `/O2` |
| `82156050` | 16 | 7 | `link_node.cpp` | - | `/O2` |
| `82548F10` | 28 | 7 | `zero5_20first.cpp` | - | `/O2` |
| `822553C0` | 24 | 6 | `eq1_2264.cpp` | - | `/O2` |
| `821A93C8` | 24 | 6 | `eq1_144_36.cpp` | - | `/O2` |
| `82649240` | 20 | 6 | `zero64_68_0.cpp` | - | `/O2` |
| `82727028` | 20 | 6 | `store_sum.cpp` | - | `/O2` |
| `821A5490` | 24 | 6 | `cmp_set.cpp` | - | `/O2` |
| `825BD930` | 16 | 6 | `bit_test.cpp` | - | `/O2` |
| `82697608` | 16 | 6 | `guard_arg3.cpp` | - | `/O2` |
| `8219FC90` | 24 | 6 | `eq1_2260.cpp` | - | `/O2` |
| `8214CC48` | 16 | 6 | `or_flag.cpp` | - | `/O2` |
| `822D4118` | 32 | 6 | `copy3_72.cpp` | - | `/O2` |
| `822D40F8` | 32 | 6 | `copy3_68.cpp` | - | `/O2` |
| `828133B8` | 28 | 6 | `two_vtables_b.cpp` | - | `/O2 /Os` |
| `8288A788` | 28 | 6 | `two_vtables.cpp` | - | `/O2 /Os` |
| `827C4FB0` | 24 | 5 | `ptr_or_null.cpp` | - | `/O2` |
| `827A7C98` | 20 | 5 | `store_two.cpp` | - | `/O2` |
| `8272CB68` | 16 | 5 | `load_global_store.cpp` | - | `/O2` |
| `825E41D8` | 16 | 5 | `zero2.cpp` | - | `/O2` |
| `822D2528` | 24 | 5 | `table624.cpp` | - | `/O2` |
| `82250B88` | 24 | 5 | `eq0_stride16.cpp` | - | `/O2` |
| `8224DF58` | 24 | 5 | `ctor_vt2.cpp` | - | `/O2` |
| `82202BC8` | 28 | 5 | `store_floats.cpp` | - | `/O2` |
| `82543F60` | 24 | 5 | `tail_or_zero.cpp` | - | `/O2` |
| `827245C0` | 28 | 5 | `ring_index.cpp` | - | `/O2` |
| `822D0BE8` | 32 | 5 | `deref_or_zero.cpp` | - | `/O2` |
| `825E3598` | 24 | 5 | `vcall_global_2.cpp` | - | `/O2 /Os` |
| `825E35C8` | 24 | 5 | `vcall_global_4.cpp` | - | `/O2 /Os` |
| `827FE808` | 16 | 5 | `and_byte.cpp` | - | `/O2 /Os` |
| `822607F0` | 120 | 2 | `grid_indices.cpp` | - | `/O2` |
| `826FE5B8` | 16 | 2 | `set_vtable.cpp` | SetVTableD170 | `/O2` |
| `826FE5C8` | 16 | 2 | `set_vtable.cpp` | SetVTableD180 | `/O2` |

---

## How these were found

The technique is unchanged and is still the whole of it: **read the target's
register discipline out of the disassembly instead of guessing plausible C.**
Which value it keeps live, and for how long, is the specification.

What made the batch work was doing it in bulk -- dump forty candidates with
their disassembly, write the twenty that read clearly, compile all twenty at
once. Roughly 70% match on the first attempt. The rest fail in ways that are
usually one edit from correct, and the edit is visible in the diff.

### Idioms worth recognising, because each one appeared several times

| what you see | what it is |
|---|---|
| `addi -1 ; cntlzw ; rlwinm rX,rY,27,31,31` | `x == 1`, branchless. Without the `addi`, `x == 0` |
| `rlwinm rX,rY,2,0,29` then `lwzx` | `array[i]` on 4-byte elements |
| `rlwinm r11,r4,1 ; add r11,r4,r11 ; rlwinm r11,r11,3` | `i * 24` built as `(i + i*2) * 8` |
| `mulli` | a stride that is not a power of two -- the number is the element size |
| `lis` + `addi` + a second `addi` | a field inside a global array element; the second addi is the field offset |
| `addi rX,rY,-1` before an update-form load or store | a biased pointer so `lbzu`/`stwu` can increment and access in one instruction |
| `mtctr` + `bdnz` | a counted loop; the `li` before `mtctr` is the trip count |
| `beqlr` / `bnelr` | a guard written as a conditional RETURN, so the body is the fall-through |
| `lwz rX,0(rY) ; lwz rX,n(rX) ; mtctr ; bctr` | a virtual call; the slot index is `n / 4` |

### Branch polarity is source order, not a flag

`beq-` jumping AWAY to a zero return means the interesting path is the
fall-through, so it must be written first:

```c
if (p) return Query(p);     /* matches   */
return 0;

if (!p) return 0;           /* does not: polarity inverts */
return Query(p);
```

Two functions in this batch failed on exactly that and matched once the
positive path was written first.

### Store order is source order

Where a function writes several fields, the emitted order is the source
order, even when it is not address order. `sub_82649240` writes 64, 68, then
0; `sub_82548F10` writes 20 then 4, 8, 12, 16; `sub_82202BC8` interleaves an
integer store between the fourth and fifth float stores. Each was written in
the target's own order and matched.


### Flags are a property of the translation unit

For a long time this file said every match was found at one uniform
`/O2 /Gy /GS- /fp:fast`, and offered that uniformity as evidence about how
the title was built. **That was wrong.** Eight functions listed here as
stalls were never stalls: they match under `/O2 /Os`, and under `/O1`, and
not under `/O2`.

The reason it took so long to see is a bad piece of reasoning that is worth
keeping. `sub_827007E8` was found to match under `/Os` early on, and the idea
was dismissed because "its two nearest neighbours are the identical idiom and
use the other register". Those neighbours are **8.5 KB away**. That is not the
same translation unit and it was never evidence of anything.

The right test is adjacency, because a translation unit is contiguous:

```
822D40F8  822D4118    /O2 only   /O2 only    AGREE
82540728  82540750    /O2 only   /O2 only    AGREE
825E3598  825E35C8    /Os only   /Os only    AGREE     48 bytes apart
82600BB0  82600BD8    /O2 only   /O2 only    AGREE
826FE5B8  826FE5C8    /O2 only   /O2 only    AGREE     16 bytes apart
827245C0  827245E0    /O2 only   /O2 only    AGREE

six informative adjacent pairs, six agreements, no split
```

Functions that sit next to each other always want the same level, and
functions far apart need not. So the level is a per-unit property, and
`src/manifest.txt` now has a `flags=` column to say so.

Thirty of the sixty-four are flag-insensitive and tell us nothing either way;
they are excluded from that count.

### Translation-unit context does NOT affect codegen

The obvious next worry was that a function's bytes might depend on what else
is in its file, which would mean matching required reconstructing whole
translation units. It does not. The same function was compiled six ways --
alone, with a companion before it, after it, both sides, and with a
register-hungry function on either side -- and produced **byte-identical code
every time**. Whatever decides register allocation is inside the function.

### The member-function lever

`sub_826C0FC8` produced the right six instructions with `r10` and `r11`
exactly TRANSPOSED, and six free-function shapes -- index, pointer
arithmetic, explicit stride, index in a local, base in a local, unsigned
index -- all gave 2 of 6. Writing it as a member function matched 6/6.

So `this` is not simply the first parameter as far as register allocation is
concerned. **When registers come out transposed and the first argument looks
like an object pointer, try the member form.**

It is a lever, not a rule: the same change was tried on four other
transposed functions and moved none of them.

---

## What still resists

Six remain, down from eleven. Eight of the original list were not stalls at
all -- they wanted `/Os` -- and one more (`sub_822D0BE8`) came down to `x > 0`
against `x != 0`, which compile to different branch conditions for an
unsigned value.

| function | bytes | the free choice |
|---|---|---|
| `82806FD0` | 84 | branch polarity -- `bgtlr` vs `ble-`, a probability decision |
| `826C1480` | 76 | instruction order -- where one store sits among five loads |
| `8215E5B0` | 28 | register assignment across an argument permutation |
| `82600AD0` | 28 | a reloaded field the compiler will not keep in a register |
| `82639C38` | 20 | an extra `mr` to keep the object alive across a float load |
| `827618E8` | 136 | loop rotation; the target keeps counts in callee-saved r30/r31 |

None of them matches at `/O2`, `/O1` or `/O2 /Os`, so the flag explanation is
exhausted for these. `tools/permuter.py` has not moved any of them either.

---

## Two facts about sizes that cost time

**The recorded size can be SHORT.** MSVC appends an unreachable `blr` after a
tail call, and a body computed from *reachable* code does not count it.
`sub_82807B38` is recorded as 16 bytes and its code is 20. `match.py`
reconciles this, bounded by the next known function start and only when the
extra words actually agree.

**A COMDAT is padded**, so trailing nops and zeros are trimmed before
comparison and what was trimmed is reported. Checked against all matches: no
real trailing instruction has ever been eaten.

---

## Out of scope, and how that was found

`sub_82917B88` matched byte for byte and then failed the build with
"falls outside .text". It is in **BINK** -- RAD's prebuilt video codec, which
is executable but is not `.text`, is middleware, and has no business being
decompiled here. `candidates.py` now excludes that range. The match is
correct and is not counted.

---

## Browsing this in objdiff

objdiff compares two OBJECT FILES per unit. This project has neither shape
lying around -- the target is a linked retail image and the base is a COFF
object -- so `tools/objdiff_export.py` synthesizes both as PowerPC ELF
relocatables and writes `objdiff.json`:

```bash
python tools/objdiff_export.py
objdiff-cli report generate -p . -o build/objdiff/report.json
```

Verified end to end against objdiff-cli 3.8.0: it reads the synthesized
objects, decodes them as PowerPC, and reports

```
units       70 total, 56 complete
functions   70 total, 56 matched
code        2008 bytes total, 1332 matched
matched     66.33%   fuzzy 85.51%
```

Two decisions worth knowing:

**The export includes the functions that do NOT match**, from
`src/attempts.txt`. A unit list where every row reads 100% shows nothing; the
near-misses are the reason to open a visual diff at all. They are kept out of
`src/manifest.txt` because that is what `build.py` verifies and a
non-matching row there would break the build.

**The base has its relocations already resolved**, as `build.py` does. A
relocation's address is chosen by the original linker and is not knowable
from source, so emitting the base un-patched would show every `bl` and every
`lis`/`addi` pair as a difference even for a function that verifies
perfectly.

**objdiff will not decode VMX128.** None of the currently matched functions
contain any -- checked, 0 of 325 instructions -- but the engine's vector
maths will not render. `tools/disasm.py` is the reader that knows it.

### The near-misses, as objdiff scores them

| unit | fuzzy match |
|---|---|
| `sub_827C5198 (vcall116)` | 98.0% |
| `sub_828864E0 (vcall_arg_adj)` | 98.0% |
| `sub_827007E8 (set_vtable_827007E8)` | 97.5% |
| `sub_8288A788 (two_vtables)` | 97.1% |
| `sub_828133B8 (two_vtables_b)` | 97.1% |
| `sub_825E35C8 (vcall_global_4)` | 96.7% |
| `sub_825E3598 (vcall_global_2)` | 96.7% |
| `sub_826C1480 (init12)` | 89.5% |
| `sub_82600AD0 (list_insert)` | 71.4% |
| `sub_82639C38 (fadd_fwd)` | 65.8% |
| `sub_827FE808 (and_byte)` | 58.8% |
| `sub_82806FD0 (chunked_at)` | 57.1% |
| `sub_827618E8 (wstr_compare)` | 38.4% |
| `sub_8215E5B0 (arg_shuffle)` | 12.1% |

---

## The permuter

`tools/permuter.py` mutates a source in ways that cannot change what it
computes, compiles each mutation with the real XDK compiler, and scores it
against the retail bytes -- the decomp-permuter idea, sized to this project.

```bash
python tools/permuter.py src/vcall116.cpp 827C5198 --iters 500
python tools/permuter.py --selftest
```

**It validates against a known answer.** `sub_826C0FC8` scores 2/6 as a free
function and 6/6 as a member; `--selftest` requires the permuter to
rediscover that, and it does, in about 8 mutations. A search tool that cannot
find an answer already known by hand has no business reporting a negative.

Mutations: `reorder` (swap adjacent independent statements), `invert` (branch
polarity), `member` (free function to member), `compare` (`x != 0` versus
`x > 0`, which compile to *different branch conditions* for an unsigned
value), `inline` (remove a local that only names a subexpression), `temp`,
`sign`.

**Two bugs it had, both found by validating rather than by running it:**
substituting the parameter name into raw text rewrote `the target's own` in a
COMMENT to `the target'this own`; and converting `sub_826C1480` to a member
silently shadowed the member `f[12]` with its parameter `int f`. Mutations
now rewrite code only, and refuse when a parameter collides with a member.

**What it has not done is crack a single stall.** Six of them are the same
shape -- a chained load where the target REUSES `r11` and we allocate fresh
registers -- and none of the seven mutations reaches register allocation.
That is the honest result, and it says the next mutation to write is one that
changes register pressure rather than statement order.
