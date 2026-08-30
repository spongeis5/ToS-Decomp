# libvorbis as FMOD built it — reconstructed, not obtained

These are **not** upstream files. `thirdparty/ogg_vorbis/` holds the pristine
libogg 1.1.3 and libvorbis 1.2.0 releases and its README's claim that they
are unmodified stays exactly true; this directory is where a file goes when
the retail image demonstrably contains a **modified** build of one of them
and the modification had to be read out of the disassembly.

They are counted as `upstream` by `tools/category.py`, which decides on the
path. That is the conservative side of a real judgement call and it is worth
stating plainly: the *bodies* are upstream text, so counting these as "read
off the disassembly" would inflate the only figure in this project that
claims any of the game has been understood. What was actually recovered here
is three facts per file, not eight hundred bytes of DSP — but those three
facts are what the bytes required, and no unmodified release produces them.

## What is here

| address | function | source | bytes |
|---|---|---|---|
| `825C1D28` | `mdct_init` | `mdct_init.cpp` | 712 |
| `825C27F8` | `mdct_clear` | `mdct_clear.cpp` | 128 |

Both matched on the first attempt once the three differences below were
applied, and neither needed a lever from `MATCHED.md`.

## The three differences, and the rule they gave

**1. It is compiled as C++, not C.** This is the whole of `mdct_init`'s
`log2n` line. MSVC's `math.h` declares `inline float log(float)` as
`(float)log((double)x)` inside its `__cplusplus` block, so
`rint(log((float)n)/log(2.f))` emits `bl log ; frsp ; lfd 2.0 ; bl log ;
frsp ; fdivs ; fadds ; bl floor ; frsp ; fctiwz` — exactly `825C1D98`
through `825C1DD0`. Compiled as C the same line is all `double`: one `fdiv`
and no `frsp`. Four independent signatures agree. The seven already-matched
`mdct.c` functions are unaffected because none of them calls libm.

**2. An unused leading parameter.** `mdct_init` takes `lookup` in r4 and `n`
in r5; `mdct_clear` takes `l` in r4. r3 is never read in either. It is a
real extra parameter and not a whole-file convention — `mdct_butterflies`
(`825C2748`) and `mdct_backward` (`825C29B8`) both take their first declared
argument in r3.

**This generalises, and it was found twice independently.** A second sweep
over `ov_read`, `_fetch_and_process_packet`, `vorbis_staticbook_unpack`,
`vorbis_staticbook_clear` and `ogg_stream_pagein` found every one of them
carrying one extra leading argument as well — and the functions that do
*not* have it are exactly the ones that never allocate on any path
(`ov_info`, `ov_comment`, `vorbis_synthesis_pcmout`/`read`, the codebook
decoders, mdct's butterflies, floor1). That set is precisely the set already
matching against pristine upstream. So:

> **In this image, a libogg/libvorbis function that transitively allocates
> or frees carries one extra leading parameter, and a function that does not
> has upstream's arity.**

That is a prediction, not a summary, and it is the most useful thing in this
directory: it says which upstream files can be matched as-is and which
cannot, before compiling anything.

**3. Allocation goes through FMOD's debug allocator.** `_ogg_malloc` and
`_ogg_free` are three-argument calls — `alloc(bytes, file, line)` at
`8252D950`, `free(ptr, file, line)` at `8252D9C8` — not the one-argument
`malloc`/`free` that `ogg/include/ogg/os_types.h` defines. The `__FILE__`
string is the image's own `..\lib\ogg_vorbis\vorbis\lib\mdct.c` and the
`__LINE__` immediates are **not relocated**, so they had to be reproduced
exactly. `mdct_init`'s two allocations are at the vendored file's own lines
53 and 54; `mdct_clear`'s are at 348 and 349, **seven more** than the
vendored 341 and 342 — the cost of the edits made to `mdct_init` above them.
`#line` reproduces the count without touching the pristine tree, and also
sets `__FILE__` to retail's path rather than a local build path, which is
what `tools/test_privacy.py` requires.

Two more things the bytes decided: `mdct_init` returns `int` (0, or **-139**
on failure) with both null guards branching forward to one shared
`li r3,-139` planted after the success epilogue — so the failure path is
written last. And `mdct_clear`'s `memset(l,0,20)` is a **real call** to the
CRT `memset` at `82301150`, which needs `#pragma function(memset)`; at `/O2`
MSVC otherwise expands it inline to five `stw`s, 24 bytes short.

## Why this matters beyond two functions

`825C1D28` was the largest **bridge** in the repository — it sat between
`vorbis_book_decodevv_add` and `mdct_butterfly_16`, both already matched, so
matching it merges a 4,916-byte span rather than adding 712 bytes.
`tools/bridge.py` is what ranked it there.

## A caveat for tools/sweep.py

`sweep.py`'s fixed `/O2` and `/O2 /Os` command lines carry no `/I`, so a
bare sweep cannot find the vendored headers and reports these as "would not
compile" rather than as matches. The manifest row's `flags=` column is what
builds them. This does not affect `verify.py`.
