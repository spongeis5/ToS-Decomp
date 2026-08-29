# Matched functions

Byte-for-byte matches against the retail image, compiled with the original
XDK 8276 toolchain (`cl.exe` 15.00.8153), every one at
`/O2 /Gy /GS- /fp:fast`.

**55 functions, 1300 bytes.** Verify all of them, plus the reconstructing build
and five negative controls, with one command:

```bash
python tools/verify.py
```

Every match is also a row in `src/manifest.txt`, so `tools/build.py` compiles
it, resolves its relocations against the retail bytes and splices it into
`.text`. Nothing here is a match on `match.py`'s word-comparison alone.

| address | bytes | callers | source | symbol |
|---|---|---|---|---|
| `82807B38` | 20 | 314 | `guard_tailcall.cpp` | - |
| `82600BD8` | 16 | 136 | `global_field.cpp` | - |
| `821A4628` | 28 | 108 | `ctor_vt.cpp` | - |
| `8253FD70` | 28 | 82 | `array_add.cpp` | - |
| `822D2450` | 24 | 63 | `table_index.cpp` | - |
| `82540750` | 28 | 49 | `string_utils.cpp` | StrCopy |
| `82540728` | 36 | 37 | `string_utils.cpp` | StrLen |
| `82677040` | 20 | 27 | `owner_clear.cpp` | ClearAndHandleOther |
| `82677028` | 20 | 27 | `owner_clear.cpp` | ClearAndHandle |
| `821636A8` | 24 | 26 | `chain5.cpp` | - |
| `826C5E00` | 28 | 19 | `vcall_global_arg.cpp` | - |
| `826A3350` | 20 | 19 | `null_tailcall.cpp` | - |
| `82600BB0` | 20 | 19 | `vcall_arg2.cpp` | - |
| `8224E7C0` | 16 | 17 | `arr_index0.cpp` | - |
| `821A4FA0` | 16 | 17 | `fwd_global.cpp` | - |
| `8224E080` | 20 | 16 | `vcall_f8_40.cpp` | - |
| `822021F8` | 16 | 14 | `stride116.cpp` | - |
| `826C0FC8` | 24 | 11 | `stride24.cpp` | - |
| `82639C28` | 16 | 11 | `chain_add48.cpp` | - |
| `82166FD0` | 16 | 11 | `fwd_vec3.cpp` | - |
| `822020B0` | 16 | 10 | `chain2_156.cpp` | - |
| `827245E0` | 32 | 9 | `ring_index2.cpp` | - |
| `8253FE28` | 28 | 9 | `zero48.cpp` | - |
| `821A4FB0` | 20 | 9 | `fwd_global_n.cpp` | - |
| `82603948` | 20 | 8 | `null_call0.cpp` | - |
| `821A5378` | 20 | 8 | `eq2_208.cpp` | - |
| `82727258` | 16 | 7 | `stride8.cpp` | - |
| `82548F10` | 28 | 7 | `zero5_20first.cpp` | - |
| `82265D30` | 20 | 7 | `set0_255.cpp` | - |
| `8225FDD8` | 20 | 7 | `zero3.cpp` | - |
| `82156050` | 16 | 7 | `link_node.cpp` | - |
| `82727028` | 20 | 6 | `store_sum.cpp` | - |
| `82697608` | 16 | 6 | `guard_arg3.cpp` | - |
| `82649240` | 20 | 6 | `zero64_68_0.cpp` | - |
| `825BD930` | 16 | 6 | `bit_test.cpp` | - |
| `822D4118` | 32 | 6 | `copy3_72.cpp` | - |
| `822D40F8` | 32 | 6 | `copy3_68.cpp` | - |
| `822553C0` | 24 | 6 | `eq1_2264.cpp` | - |
| `821A93C8` | 24 | 6 | `eq1_144_36.cpp` | - |
| `821A5490` | 24 | 6 | `cmp_set.cpp` | - |
| `8219FC90` | 24 | 6 | `eq1_2260.cpp` | - |
| `8214CC48` | 16 | 6 | `or_flag.cpp` | - |
| `827C4FB0` | 24 | 5 | `ptr_or_null.cpp` | - |
| `827A7C98` | 20 | 5 | `store_two.cpp` | - |
| `8272CB68` | 16 | 5 | `load_global_store.cpp` | - |
| `827245C0` | 28 | 5 | `ring_index.cpp` | - |
| `825E41D8` | 16 | 5 | `zero2.cpp` | - |
| `82543F60` | 24 | 5 | `tail_or_zero.cpp` | - |
| `822D2528` | 24 | 5 | `table624.cpp` | - |
| `82250B88` | 24 | 5 | `eq0_stride16.cpp` | - |
| `8224DF58` | 24 | 5 | `ctor_vt2.cpp` | - |
| `82202BC8` | 28 | 5 | `store_floats.cpp` | - |
| `826FE5C8` | 16 | 2 | `set_vtable.cpp` | SetVTableD180 |
| `826FE5B8` | 16 | 2 | `set_vtable.cpp` | SetVTableD170 |
| `822607F0` | 120 | 2 | `grid_indices.cpp` | - |

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

Every one of these has the right instructions in the right multiset and
differs only in a choice made inside the compiler.

| function | bytes | best | the free choice |
|---|---|---|---|
| `82806FD0` | 84 | 11/21 | branch polarity -- `bgtlr` vs `ble-`, a probability decision |
| `826C1480` | 76 | 13/19 | instruction order -- where one store sits among five loads |
| `827C5198` | 20 | 3/5 | register assignment -- `r11` reused vs a fresh `r10` |
| `828864E0` | 20 | 3/5 | the same transposition |
| `827007E8` | 16 | 1/4 | the same, and its two neighbours use the OTHER register |
| `8215E5B0` | 28 | 1/7 | register assignment across an argument permutation |
| `8288A788` | 28 | 2/7 | `addi` results in `r9`/`r8` where the target reuses `r11`/`r10` |
| `827FE808` | 16 | 1/4 | `andi.` in one instruction vs a zero-extend and a mask in two |
| `82639C38` | 20 | 1/5 | an extra `mr` to keep the object alive across a float load |
| `822D0BE8` | 32 | 3/8 | branch polarity, and it survived writing the positive path first |
| `827618E8` | 136 | partial | loop rotation; the target keeps counts in callee-saved r30/r31 |

That is the honest shape of the remaining difficulty: not semantics, not
struct layout, but the last decision the compiler makes on its own.
`decomp-permuter`-style automatic source mutation is the tool built for
exactly this and is the obvious next thing to reach for.

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
