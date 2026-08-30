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
| `825BF448` | `vorbis_synthesis_blockin` | `vorbis_synthesis_blockin.c` | 2472 |
| `825BDFB0` | `ogg_stream_pagein` | `ogg_stream_pagein.c` | 1064 |

and the reconstructed headers the last two need:

| header | reconstructed from | what differs |
|---|---|---|
| `bitrate.h` | `vorbis/lib/bitrate.h` | every `double` is a `float` (3) |
| `highlevel.h` | `vorbis/lib/highlevel.h` | every `double` is a `float` (16) |
| `codec_internal.h` | `vorbis/lib/codec_internal.h` | nothing — a copy, so the include chain starts here |
| `ogg/ogg.h` | `ogg/include/ogg/ogg.h` | `lacing_vals` 16-bit, `granule_vals` 32-bit |

The mdct pair matched on the first attempt once the three differences below
were applied, and neither needed a lever from `MATCHED.md`. `ogg_stream_pagein`
also matched on the first attempt, from the prediction below plus the two
member types. `vorbis_synthesis_blockin` did not, and cost one lever — see
its own header comment.

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

**It has now been used as one.** `ogg_stream_pagein` (`825BDFB0`)
reallocates, so the rule says it takes the extra argument; it was written
that way before compiling and matched 254 of 254 non-relocated words on the
first attempt. `vorbis_synthesis_blockin` (`825BF448`) allocates nothing on
any path, so the rule says upstream's arity, and two arguments is what it
took. Two predictions, two confirmations, nothing adjusted afterwards.

## The headers are modified too, and that is measurable

The rule above is about signatures. Two struct layouts differ as well, and
they are pinned by offsets read straight out of already-identified code
rather than guessed:

    codec_setup_info.halfrate_flag   3532   (825BF470)
    private_state.sample_count        120   (825BF4D4, 825BFC84, 825BFCF0)
    ogg_stream_state.lacing_vals     2-byte elements   (825BE17C, 825BE1D4)
    ogg_stream_state.granule_vals    4-byte elements   (825BE198, 825BE388)

Pristine 1.2.0 puts the first two at 3656 and 128 — and so does 1.2.2, so the
release is not what moves them. Both sit behind encoder-only sub-structures.
Dropping every `double` in `bitrate.h` and `highlevel.h` to a `float` gives
`sizeof(bitrate_manager_state)` 48 → 40 and `sizeof(highlevel_encode_setup)`
264 → 148 with `bitrate_manager_info` 32 → 24, which lands **both** offsets
exactly, from one hypothesis with no free parameter. Fitting one offset would
prove nothing; hitting two independent ones is why it is written down.

The libogg pair is read off element scaling and access width — a `*2` in the
reallocation and `lhzu`/`sthx` throughout for the lacing array against a `*4`
and `stwx` for the granule array — and costs no guesswork at all. Every
struct offset is unchanged either way, because only pointee types move.

**What the bytes do not say:** only those four facts are measured. Any other
layout summing the same way would be indistinguishable, and no decoder-side
code in the image reads a field of `bitrate_manager_state`,
`bitrate_manager_info` or `highlevel_encode_setup`.

## A caveat for tools/link.py

`ogg_stream_pagein.c`'s object defines **ten** functions, not one: the seven
`ogg_page_*` accessors and the two `_os_*_expand` helpers are `static` and
fully inlined into the body, but `/Gy` emits a COMDAT for each anyway, and
`__forceinline` does not change that (tested; the bytes of
`ogg_stream_pagein` are identical either way, so the simpler text is what is
here). Three of those names — `ogg_page_continued`, `ogg_page_eos`,
`ogg_page_granulepos` — are already named by `src/manifest.txt` at
`825BD920`, `825BD940` and `825BD950` from the pristine `framing.c`.

**It is not currently a problem, and the reason is worth stating exactly.**
`link.py` links each contiguous RUN separately, and these two objects are in
different runs — `825BD920` and `825BDFB0` — so `link.exe` is never handed
both at once. The `825BD920` run does fail to link, but for an unrelated
reason that was invisible until the diagnostic was fixed: **`LNK2013: REL24
fixup overflow, target 'memset' is out of range`**. A run placed at its
retail address cannot reach a CRT function that is not placed with it, and
that is a 24-bit branch limit, not a symbol conflict. (`825AA548` fails the
same way on `fseek`.)

So: whether the duplicates collide is a question for the WHOLE-IMAGE link,
where every object is handed over together, and it is still NOT_MEASURED.
It also means this unit cannot be `complete` until the manifest names all
ten functions — `link.py` owns that question and answers it honestly today.

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
