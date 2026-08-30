# ToS-Decomp — orientation

A byte-matching decompilation of *SpongeBob's Truth or Square* (Xbox 360).
Source is written, compiled with the title's own 2008 XDK `cl.exe`
15.00.8153, and must produce bytes **identical** to the retail image.

This file is deliberately short. It loads automatically; the documents it
points at do not, and they are where the knowledge is.

## Read before working

1. **`MATCHED.md`** — the idiom table and the ~20 measured **levers**. Nearly
   every match is won by applying one of them. This is the highest-value
   reading in the repository and skipping it means rediscovering things that
   took days.
2. **`SHELL-TRAPS.md`** — this environment silently corrupts files. A
   backslash in a bash heredoc arrives mangled; a hook now refuses those, and
   it has fired repeatedly. Write scripts with the Write tool and run them.
3. **`HANDBOOK.md`** — full state, setup, the tool inventory, and where to
   pick up. The long one.

## The loop

```bash
python tools/verify.py                 35 checks; ~5 min. Run it first.
python tools/batch.py 40 --no-vmx      candidates, ranked by CALLER COUNT
python tools/match.py <src> <addr>     compile one and compare
python tools/sweep.py                  recover work whose row was never written
python tools/sweep.py --attempts       live near-miss scores, both flag levels
python tools/bridge.py                 which unmatched function would MERGE two runs
python tools/link.py --list            which adjacent runs the real linker can take
python tools/link.py --units           which source files are COMPLETE (= linked)
```

Parallel agents work well on batches. Give each a distinct filename prefix —
they share `src/`. Re-run `match.py` on every claim yourself; `build.py` is
stricter still, because it RESOLVES relocations rather than excusing them.

**`build.py` is a splice; `link.py` is a link.** The splice writes each
function at the address the manifest names, so it cannot see whether two
functions PACK, what is in the PADDING between them, or whether the ORDER is
reachable — our objects do not even hold them in retail order. `link.py` hands
contiguous runs to the retail `link.exe`, placed at their retail addresses.

**So adjacency is worth more than isolated matches, and `bridge.py` ranks by
it.** A function between two matched runs does not add its own bytes, it adds
the merged span: three string routines worth 228 bytes turned a 168- and a
364-byte run into one 768-byte run covering a whole translation unit.

**`complete` means LINKED, not matched** — it is a separate figure in
objdiff's schema and on decomp.dev, and it was reported wrong in the
flattering direction for months. A unit is complete when its object defines
no function the manifest does not name AND every one of them is in a linked,
placed, byte-identical run. `link.py` owns that question; `report.py` and
`objdiff_export.py` import the answer rather than deciding it.

## Rules that are not negotiable

**Never weaken, bypass, or add a skip flag to a check.** Every one exists
because the rule it enforces was broken while written down.

**Any tool that decides "does this match?" must import `can_shrink` and
`can_extend` from `match.py`.** Five have now disagreed with `verify.py` by
reimplementing that comparison, always in the direction that gets believed.

**And any tool that picks WHICH function a manifest row means must import
`pick_function` from `libmatch.py`.** Five had their own copy; two omitted
the exact-name test between the mangled test and the substring test, and
since C symbols are not mangled and `vorbis_book_decode` is a prefix of
`vorbis_book_decodev_add`, one of them measured a 100-byte function as 572
and inflated the published byte count by 488.

**Figures in prose rot; generate them or point at the generated block.**
`readme_stats.py`, `matched_table.py` and `tool_table.py` own every count
that appears in a document, and `verify.py` fails when one drifts. The tool
inventory reached 27 of 77 missing before it was generated, and the README
front page was once wrong by six times.

**State the denominator on every count.** Not "24 draws" but "24 draws of 59
packets walked". A fact that could not be measured is not zero.

**A tool that failed must not report a benign value.** Absence of evidence
rendered as evidence of absence is the most expensive failure mode here.

**Read output end to end.** `head`, `tail` and `sed -n` ranges are not
reading a log; a filter returns only what was already suspected.

**Nothing personal in tracked files or commits.** `tools/test_privacy.py`
enforces it and `hooks/pre-commit` blocks a commit that would break it
(`git config core.hooksPath hooks` after any fresh clone). Content can be
edited away; an identity in git history cannot.

## The principle this project keeps re-proving

A measurement of what the compiler did is **evidence**. A conclusion that
*nothing can be done about it* has so far **always** been wrong — both
"provably impossible" claims fell, along with **seven** stalls that had a
recorded mechanism saying why they were unreachable. The measurements were
right; the conclusions drawn from them were not. Nothing has ever fallen the
other way.

The newest four, all in one afternoon: `82606EC8` and `82606FD8` (a
folded-away read still sets operand order — read the base twice in a form
that cancels, and nothing stays live), `8216C240` (the AND-mask lever applied
to the INDEX, not the operator — it defeats MSVC's `base + (index << scale)`
pattern and the rebuilt tree emits the commutative `or` the other way round),
and `825DB4C0` (three separate levers each reach it, so the bytes do not say
which was written).

So a near-miss with a recorded reason is not finished. It is the more
promising target, because the reason names the lever to reach for. Do not
write "provably" here.
