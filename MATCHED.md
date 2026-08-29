# Matched functions

Byte-for-byte matches against the retail image, compiled with the original
XDK 8276 toolchain (`cl.exe` 15.00.8153).

Verify any row with:

    python tools/match.py <source> <address>

| address | bytes | callers | source | what it is | compared |
|---|---|---|---|---|---|
| `82807B38` | 20 | **314** | `guard_tailcall.cpp` | guarded tail call | 4/5, 1 reloc |
| `82600BD8` | 16 | 135 | `global_field.cpp` | read a global's field | 2/4, 2 reloc ⚠ |
| `821A4628` | 28 | 108 | `ctor_vt.cpp` | vtable + two zeroed fields | 5/7, 2 reloc |
| `8253FD70` | 28 | 82 | `array_add.cpp` | guarded accumulate | 7/7 |
| `822D2450` | 24 | 59 | `table_index.cpp` | field of a global array element | 4/6, 2 reloc |
| `82540750` | 28 | 49 | `strcopy.cpp` | byte copy to NUL (`strcpy`) | 7/7 |
| `82540728` | 36 | 37 | `strlen.cpp` | string length (`strlen`) | 9/9 |
| `821636A8` | 24 | 26 | `chain5.cpp` | five dependent loads | 6/6 |
| `82677028` | 20 | 25 | `clear_and_call.cpp` | clear a field, tail-call | 4/5, 1 reloc |
| `82677040` | 20 | 25 | `clear_and_call2.cpp` | the same, field 140 | 4/5, 1 reloc |
| `826A3350` | 20 | 19 | `null_tailcall.cpp` | load, null-check, tail-call | 4/5, 1 reloc |
| `82600BB0` | 20 | 19 | `vcall_arg2.cpp` | virtual call on arg 2 | 5/5 |
| `822607F0` | 120 | 2 | `grid_indices.cpp` | grid strip indices | 30/30 |
| `826FE5C8` | 16 | 2 | `set_vtable.cpp` | store a fixed address | 2/4, 2 reloc ⚠ |
| `826FE5B8` | 16 | 2 | `set_vtable.cpp` | the same, other vtable | 2/4, 2 reloc ⚠ |

**15 of 25,737 functions**, every one at `/O2 /Gy /GS- /fp:fast` — a single
uniform flag set, which is itself evidence about how the title was built.

"compared" states its own denominator: a word the linker patches is masked,
because an object refers to symbols by placeholder and counting a relocation
as a mismatch would make a correct function look wrong.

⚠ marks matches where only two words are non-relocated and both are trivial
(a store and a `blr`). The shape is confirmed; the addresses are not. These
are the weakest rows here and should not be leaned on.

`python tools/candidates.py` narrows the target set to 2,565 vetted leaves.

## One match that was nearly banked and should not have been

`sub_827007E8` is the same idiom as `826FE5C8`/`826FE5B8` —
`lis / addi / stw / blr` — and it does **not** match at `/O2`, because the
target reuses `r11` where we allocate `r10`. Adding `/Os` makes it match.

It was very nearly recorded as a match on that basis. It is not one:

* only **2 of 4 words** are non-relocated, and both are trivial, so `/Os`
  had to make almost nothing agree;
* `/Os` breaks 7 of the other matches, so it is not the build's flag set;
* and decisively, its two nearest neighbours in the image are the *identical
  idiom* and use `addi r10` — the `/O2` form. Same neighbourhood, both
  register choices.

So the register choice varies for reasons internal to the compiler, and
picking a flag that reproduces two trivial words is fitting the flag to the
answer. **A flag chosen per-function to force a match on few compared words
is not evidence, and the neighbourhood is the check that catches it.**

---

## What separates the six from the four

Five of the six above matched **on the first attempt**, while two earlier
functions absorbed eight source shapes, 65 flag combinations and days without
matching. It is worth being precise about why, because the obvious explanation
is wrong.

**What the four stalls have in common** is that in each one the compiler made
a **free choice the source cannot express**:

| function | the free choice |
|---|---|
| `826C1480` | instruction order — where one store sits among five loads |
| `82806FD0` | branch polarity — `bgtlr` vs `ble-`, a probability decision |
| `827C5198` | register assignment — `r11` reused vs a fresh `r10` |
| `8215E5B0` | register assignment — a different permutation, same semantics |

In every one of them the *semantics are already settled* and the instruction
multiset is right. What differs is a decision made inside the compiler, below
the level the language reaches.

**This is diagnostic, not predictive.** A first attempt to turn it into a
ranking — "prefer functions whose instructions each depend on the previous
one", implemented in `tools/serial.py` — was validated against all ten
outcomes and **failed**:

```
822607F0   MATCHED        serial 0.03    <- the LOWEST score of the ten
827C5198   stalled 3/5    serial 0.75    <- the second HIGHEST
```

The metric measures the opposite of what it was meant to. An optimising
compiler deliberately separates dependent instructions to hide latency, so
well-scheduled code has low adjacent dependency *by construction*. `serial.py`
is kept only because `--validate` is the record of that refutation; it prints
its own verdict on every run so it cannot quietly become a filter.

**What did work, and is the actual technique** (§7d): *read the target's
register discipline out of the disassembly instead of guessing plausible C.*
Which value it keeps live, and for how long, IS the specification. All six
matches were written that way, straight off the listing — `sub_821636A8` is
the clean demonstration, five dependent `lwz` and a `blr`, written in about a
minute.

Small helps, but not decisively: the matched ones are 20–120 bytes and
`827C5198` stalled at 20.

---

## Two facts about sizes that cost time

**The recorded size can be SHORT.** `sub_82807B38` is recorded as 16 bytes and
its code is 20. MSVC appends an unreachable `blr` after a tail call, and a
body computed from *reachable* code does not count it. `match.py` reported NO
MATCH for a byte-perfect source.

`match.py` now reconciles this: when our code is longer, it extends the window
into the image — bounded by the next known function start — and reports the
reconciliation explicitly. It never extends silently, and only when the extra
words actually agree.

Censused over the whole inventory:

```
25,558 functions examined
11,787 end in an unconditional branch (tail calls)
   171 are followed immediately by a blr   <- recorded size short by 4 (0.67%)
 5,971 are followed by a zero pad word     <- size is right
```

**A COMDAT is padded**, so trailing nops and zeros are trimmed before
comparison and what was trimmed is reported.

---

## In progress

### `sub_827C5198` — virtual call through a member, 20 B, 48 callers

`src/vcall116.cpp`, `src/vcall116_variants.py`. **3 of 5 words.** The entire
difference is which register holds the vtable slot:

```
target:  lwz r11,0(r3) ; lwz r11,64(r11) ; mtctr r11 ; bctr
ours:    lwz r11,0(r3) ; lwz r10,64(r11) ; mtctr r10 ; bctr
```

The target reuses `r11`; we allocate a fresh `r10`. Seven source shapes —
explicit vtable, no local, byte-offset cast, a real C++ class with seventeen
virtual functions, void return, member function — all give exactly 3/5. This
is register allocation, not semantics.

### `sub_8215E5B0` — argument reshuffle into a tail call, 28 B, 26 callers

`src/arg_shuffle.cpp`. **1 of 7 words.** The semantics are settled — the moves
are a permutation, so the call is `Callee(c, a->first, b, 0)` from
`f(a, b, c)` — and ours computes exactly that with a different register
assignment. Same class as the above.

### `sub_82806FD0` — chunked-container element accessor, 84 B, 220 callers

`src/chunked_at.cpp`, `src/chunked_at_variants.py`. **11 of 21 words**, at
exactly 84 bytes. First instruction and last six match. Semantics settled: the
two `rlwinm` decode as `(i >> 5) * 4` and `(i & 31) * 16`, so a chunk holds 32
elements of 16 bytes, and the bounds test is unsigned with a null return.

The remaining difference is one branch-probability decision:

```
target:  cmplw cr6,r10,r9 ; bgtlr cr6        RETURN when i > total
ours:    cmplw cr6,r10,r4 ; ble- cr6,body    branch TO body
```

`ble-` carries the not-taken hint, so this compiler assumed the guard usually
FAILS while the retail build assumed it usually PASSES.

Tried and rejected, all at exactly 84 bytes: 8 source shapes (10–11/21), a
flag sweep of 5 optimisation levels x 13 combinations (11/21 at `/O1`),
aliasing through one pointer type (10/21).

### `sub_826C1480` — 12-field initialiser, BRANCHLESS, 76 B, 180 callers

`src/init12.cpp`, `src/init12_variants.py`. **13 of 19 words**, at exactly 76
bytes, same 19 instructions in the same store order. Chosen specifically to
test whether branch layout was the wall. It is not.

```
target:  ... 5 loads ... ; stw r6,8(r3) ; lwz r6,124(r1) ...
ours:    stw r6,8(r3) ; ... 5 loads ... ; lwz r6,124(r1) ...
```

The target's rule is visible: **store each register as late as possible,
immediately before reloading it.** `stw r6` sits directly before `lwz r6`.
Ours stores early.

Writing the assignments in the target's own field order (3,4,5, 2, 0,1,
6..11 — read straight off the disassembly) took it from 10/19 to 13/19.
Nothing has moved it since:

| approach | result |
|---|---|
| 72 flag combinations including `/Ou` (prescheduling) | 13/19, **all identical** |
| 11 source shapes | 13/19, all identical |
| `__restrict`, `__declspec(noalias)`, distinct field types | 13/19 |

**The aliasing hypothesis was tested and refuted.** The target hoists five
loads *above* a store to memory, which a compiler will only do if it can prove
they cannot alias — and the loads read the parameter home area in the
caller's frame, which an `S*` argument could point at. So `__restrict` looked
like the answer. It changes nothing.

A control confirmed the sweep is real: `/Od` gives 180 bytes and 0/19, so the
flags do reach the compiler. Every optimising combination converges.

### LTCG — RESOLVED, and it is not the cause

`/GL` was tested directly:

```
without /GL :   626 B object, machine 01F2, 1 PowerPC function, 76 code bytes
with    /GL :  4737 B object, machine 0000, 0 PowerPC functions,  0 code bytes
```

If the retail build had used it for the game's own code, per-object comparison
would be the wrong methodology entirely. It did not:

```
of 1,528 objects in the retail image carrying a code-producing stamp,
1,474 (96.5%) were compiled WITHOUT /GL.  54 were.  No C TU used /GL.
```

Read out of the image's own Rich header, whose product ids were **measured**
against this XDK rather than looked up — FINDINGS §7l. Cross-checked: all 610
library objects that `libmatch.py` matched byte-for-byte carry a plain
C/C++/asm stamp, none a `/GL` one.

The decisive point is simpler than the statistics. Six functions have now
matched **as compiled objects**, all of them at addresses inside the game's
own bands.

What the 54 `/GL` objects are is NOT_MEASURED — either the title's own
translation units or members of `xact3ltcg.lib`/`x3daudioltcg.lib`, the only
two LTCG libraries without a matched twin.

### PGO — no longer needed to explain anything

The competing hypothesis was profile-guided optimisation. The branchless
`sub_826C1480` refutes it as the cause: branch-probability data cannot affect
a function with no conditional branch, and it still will not match.

Whether the title used PGO at all is NOT_MEASURED. `/LTCG:PGI` fails here with
`LNK1123: failure during conversion to COFF` on the `.pgd`, so the product id
a PGO build stamps is unknown and the image cannot be checked for it.

### `sub_827618E8` — counted wide-string compare

136 bytes, 3 callers. `src/wstr_compare.cpp`. Right size, right instruction
mix, wrong loop rotation: the target keeps the original counts in callee-saved
r30/r31 across the loop and rotates the exit test differently. 3 attempts.
