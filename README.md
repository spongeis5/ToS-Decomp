# ToS-Decomp

A **byte-matching decompilation** of *SpongeBob's Truth or Square* (Xbox 360,
Heavy Iron Studios / THQ, 2009).

The goal is C++ that, compiled with the title's own 2008 Xbox 360 XDK
compiler, produces bytes **identical** to the retail image. Not a port, not a
re-implementation, not an emulator.

**1,200 functions matched** — 382 written by hand from the disassembly, 818 generated from their own encodings — reproducing 34,096 of the 8,467,964 bytes in `.text`, 0.4026%.

Those two halves are deliberately never added up without being split. The
generated ones are a single expression each — a constant return, a field
accessor, a vtable forwarder — written by script from the instructions
themselves. They are real matches and a link will need every one, but they
say far less than the hand-written ones about how much of this game has
actually been read.

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

Twenty-six checks. Six of them are **negative controls**: each corrupts one
fact — a struct offset, a switch case mapping, a manifest address — and
requires the build to *fail*. A control that stops failing is the serious
result, because it means a check reports success without being able to see
the failure it exists to catch.

**It is a splice, not yet a link.** Matched functions are compiled and
spliced into a copy of `.text`; everything else is copied from the original.
The hash proves the rebuilt bytes are right and says nothing about the rest.

## Browsing it

[objdiff](https://github.com/encounter/objdiff) opens this directory
directly — `objdiff.json` is generated and lists every unit, with
hand-written, generated, near-miss and not-yet-started tracked as separate
progress categories.

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
both of this project's "provably impossible" claims fell, along with three
stalls that had a recorded mechanism explaining why they could not be solved.
The measurements were right every time; the conclusions drawn from them were
not.

So a function recorded as stuck, with a reason, is not finished. It is the
more promising target, because the reason tells you which lever to reach for.

## License

MIT, for the tools, build system, documentation and sources written here.
It cannot and does not grant any right to the game, to the middleware it
links against, or to `thirdparty/disasm/` (GNU binutils, GPL). See `LICENSE`.
