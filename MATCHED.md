# Matched functions

Byte-for-byte matches against the retail image, compiled with the original
XDK 8276 toolchain (`cl.exe` 15.00.8153).

Verify any row with:

    python tools/match.py <source> <address>

| address | bytes | source | symbol | flags |
|---|---|---|---|---|
| `822607F0` | 120 | `src/grid_indices.cpp` | `BuildGridStripIndices` | `/O2 /Gy /GS- /fp:fast` |

**1 of 25,737 functions.** `python tools/candidates.py` narrows the target set
to 2,565 vetted leaves; 2,095 of those are also call-graph leaves with at least
one caller.

---

## In progress

### `sub_82806FD0` — chunked-container element accessor

84 bytes, **220 callers**, calls nothing. `src/chunked_at.cpp`.

**Best: 11 of 21 words, at exactly 84 bytes.** The first instruction and the
last six match. The semantics are settled — the two `rlwinm` decode as
`(i >> 5) * 4` and `(i & 31) * 16`, so a chunk holds 32 elements of 16 bytes,
and the bounds test is unsigned with a null return.

**The whole remaining difference is one branch-probability decision:**

```
target:  cmplw cr6,r10,r9 ; bgtlr cr6        RETURN when i > total; body falls through
ours:    cmplw cr6,r10,r4 ; ble- cr6,body    branch TO body; return-0 falls through
```

`ble-` carries the not-taken hint, so this compiler assumed the guard usually
FAILS while the retail build assumed it usually PASSES. Everything else in the
diff follows from that: presetting `r3 = 0` for the conditional return is what
forces `this` into r10 and defers the `self->base` load.

Tried and rejected, all at exactly 84 bytes:

| approach | best |
|---|---|
| 8 source shapes (guard/ternary/single-exit/inverted/signed/char*) | 10-11/21 |
| flag sweep, 5 optimisation levels x 13 combinations | 11/21 (`/O1`) |
| aliasing: all reads through one pointer type | 10/21 |

**Leading hypothesis: the retail build used PGO.** Branch probability is not
reachable from source shape or from any flag tried, and the XDK ships the
tooling — `pgomgr.exe` and `pgodb90.dll` are present and `link.exe` imports
`pgodb90.dll`. Whether *this title* used it is NOT_MEASURED. If it did,
functions whose layout depends on measured branch frequencies will not match
without the profile data, which is not recoverable from the image.

Worth testing on a function with no conditional branch at all: if straight-line
functions match cleanly and branchy ones stall at this same wall, that is
strong evidence for PGO and it changes what "matchable" means for this project.

### `sub_826C1480` — 12-field initialiser, BRANCHLESS

76 bytes, **180 callers**, calls nothing, **no conditional branch at all**.
`src/init12.cpp`. Chosen specifically to test whether branch layout is the
wall.

**Best: 13 of 19 words, at exactly 76 bytes.** Same 19 instructions as the
target, same store order, differing only in where one store sinks:

```
target:  ... 5 loads ... ; stw r6,8(r3) ; lwz r6,124(r1) ...
ours:    stw r6,8(r3) ; ... 5 loads ... ; lwz r6,124(r1) ...
```

Writing the assignments in the target's own field order (3,4,5, 2, 0,1,
6..11 — read straight off the disassembly) took it from 10/19 to 13/19. The
remaining difference is where the compiler schedules one store against five
loads. A flag sweep over 5 optimisation levels x 14 combinations gives 13/19
everywhere.

**This weakens the PGO hypothesis.** A function with no conditional branch
cannot be affected by branch-probability data, and it still will not match.
The wall on both attempts is INSTRUCTION SCHEDULING, not branch layout.

### LTCG, and what it would mean

`/GL` was tested directly:

```
without /GL :   626 B object, machine 01F2, 1 PowerPC function, 76 code bytes
with    /GL :  4737 B object, machine 0000, 0 PowerPC functions, 0 code bytes
```

**With LTCG the object holds intermediate language and no machine code at
all** — codegen happens at link time. If the retail build used it for the
game's own code, per-object byte comparison is the wrong methodology and
matching would have to be done against linked output.

Evidence both ways, none conclusive:

* FOR: the build config is `Xbox 360MasterWAD`; the XDK ships 11 LTCG library
  variants; and link-time codegen would explain a scheduling difference that
  no source shape or flag reaches.
* AGAINST: 6,332 functions match **non-LTCG** XDK library objects byte for
  byte. Had those libraries been regenerated at link time they would not.
  That proves the LIBRARIES were linked without LTCG; it says nothing about
  the game's own objects, which `/LTCG` would regenerate while leaving
  precompiled libraries alone.

An attempt to compare LTCG against non-LTCG *linked* output was inconclusive:
both produced 0/19 in a raw window search, including the non-LTCG build that
scores 13/19 at object level, so the harness is wrong rather than the answer
being no. Comparing linked output needs relocations resolved properly.

**This is the most important open question in the project.** It decides
whether byte-matching at object level is viable at all for the game's own
code.

### `sub_827618E8` — counted wide-string compare

136 bytes, 3 callers. `src/wstr_compare.cpp`. Right size, right instruction
mix, wrong loop rotation: the target keeps the original counts in callee-saved
r30/r31 across the loop and rotates the exit test differently. 3 attempts.
