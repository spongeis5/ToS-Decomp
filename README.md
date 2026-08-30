Truth or slop. SpongeBob's 10 sloppiest slopments. Sloppy sloppens, hooray! 
Funneling the game through claude over and over until it produces exact an exact C++ binary accurate decomp. I'll merge just about anything using any method as long as it works and compiles and gets us closer, I don't really care. Slop through it. 

# ToS-Decomp

A **byte-matching decompilation** of *SpongeBob's Truth or Square* (Xbox 360,
Heavy Iron Studios / THQ, 2009).

The goal is C++ that, compiled with the title's own 2008 Xbox 360 XDK
compiler (XDK 8276), produces bytes **identical** to the retail image. Not a port, not a
re-implementation, not an emulator.

**1,297 functions matched** — 448 written by hand from the disassembly, 818 generated from their own encodings, 31 upstream libogg/libvorbis — reproducing 49,820 of the 8,467,964 bytes in `.text`, 0.5883%.

Those three parts are deliberately never added up without being split.

The **generated** ones are a single expression each — a constant return, a
field accessor, a vtable forwarder — written by script from the instructions
themselves. The **upstream** ones are libogg 1.1.3 and libvorbis 1.2.0,
obtained rather than recovered: the game's audio middleware vendored them, and
the release pair was identified by compiling candidates and counting what the
image contains.

Both reproduce the image exactly — the compiler does not care where source
came from, and a link needs every one. Only the hand-written count says
anything about how much of this game has actually been *read*.

## What is here, and what is not

This repository contains **no part of the game**. The retail image, the
ripped disc and the Xbox 360 XDK are absent and gitignored, and every tool
refuses to run without them. You need your own legally obtained copy and the
XDK; `HANDBOOK.md` says exactly which files and how to check a candidate copy.

Disassembly listings appear in comments throughout `src/` because a matching
decompilation is unreadable without them.

## Verifying it

One command does everything — compiles every source with the original
compiler, resolves each relocation against the retail bytes, splices the
result into `.text`, hashes the section, and runs the tool self-tests:

```bash
python tools/verify.py
```

32 checks. Several are **negative controls**: they corrupt one fact
— a struct offset, a switch case mapping, a manifest address, the order two
functions are linked in — and require the result to *fail*. A control that
stops failing is the serious result, because it means a check reports success
without being able to see the failure it exists to catch.

**The build is a splice; the link is real but partial.** Matched functions are
compiled and spliced into a copy of `.text`, and everything else is copied
from the original — so the hash proves the rebuilt bytes are right and says
nothing about the rest.

Separately, `python tools/link.py` hands contiguous runs of matched functions
to the retail linker itself, has it place them at their retail addresses, and
compares what it emits against the image:

```
124 of 181 runs, 12,100 bytes — placed, ordered and padded by link.exe
```

That tests three things a splice cannot, because a splice writes each function
at the address it was told: whether the functions **pack**, what is in the
**padding** between them, and whether the **order** is reachable at all — our
objects do not even hold them in retail order.

The 57 runs it cannot link are counted, not omitted. Most call a function
nobody has written yet, and the linker will not resolve a call to a symbol
that has no section — so the next structural step is a link that spans more
than one run. `HANDBOOK.md` says what that needs.

Note the blocked count grows as matching does, and that is expected: a new
match can join a clean run to one that calls outside itself, and the merged
run is then blocked. Linked bytes and blocked runs both go up.

### `complete` means linked, not matched

decomp.dev shows a matched percentage and a separate **complete** one, and in
objdiff's schema `complete` means the object is *linked*. This project reports
it that way: a unit counts as complete only when its object defines no
function the manifest does not name **and** every one of them was placed by
`link.exe` at its retail address, byte-identical. `tools/link.py` owns that
question and the report imports the answer.

It was not always right. The flag used to be set to "this function's bytes
match", so the published report claimed every matched byte was linked while
`build.py` printed *this is a SPLICE, not yet a LINK* on every run. `FINDINGS.md`
§7z has the whole account, including the three other counting errors the same
cross-check turned up in an hour.

## Browsing it

[objdiff](https://github.com/encounter/objdiff) opens this directory
directly — `objdiff.json` is generated and lists every unit, with
hand-written, generated, upstream, near-miss and not-yet-started tracked as
separate progress categories.

## Documentation

| file | what it is |
|---|---|
| **CLAUDE.md** | orientation — the loop, and the rules that are not negotiable |
| **HANDBOOK.md** | the working document — state, setup, tools, and where to pick up |
| **MATCHED.md** | every match, and the ~20 measured *levers* that produce them |
| **FINDINGS.md** | every established fact, with how it was measured |
| **SHELL-TRAPS.md** | ways this environment silently corrupts files |
| **VMX128.md** | a verified VMX128 reference, including errata in both public sources |

If you are here to work on it, read `MATCHED.md`'s levers first. Nearly every
match is won by applying one of them, and they are the bulk of what this
project actually knows.

## One principle, learned the hard way

A measurement of what the compiler did is evidence. A conclusion that
*nothing can be done about it* has, so far, always turned out to be wrong —
both of this project's "provably impossible" claims fell, along with **seven**
stalls that had a recorded mechanism explaining why they could not be solved.
The measurements were right every time; the conclusions drawn from them were
not. Nothing has ever fallen the other way.

The clearest of the seven: two arena allocators resisted because the target
reads a global's base *before* a guard, and every way of reading it early kept
a value live across the branch, which made the compiler merge two tails that
must stay duplicated. True, and not a bound — the read does not have to
*survive*. Writing the capacity test as a pointer difference reads the base
twice and then folds both reads away entirely, leaving nothing live and the
operand order set. One line, and its twin took the identical change untouched.

So a function recorded as stuck, with a reason, is not finished. It is the
more promising target, because the reason tells you which lever to reach for.

## License

MIT, for the tools, build system, documentation and sources written here.
It cannot and does not grant any right to the game or to the middleware it
links against. Two vendored trees carry their own terms and are not covered:
`thirdparty/disasm/` (GNU binutils, GPL) and `thirdparty/ogg_vorbis/`
(Xiph.Org libogg/libvorbis, BSD 3-clause, unmodified). See `LICENSE`.
