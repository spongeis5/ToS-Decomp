# libogg 1.1.3 + libvorbis 1.2.0, unmodified

Upstream source from the Xiph.Org Foundation, vendored so the matches that
use it can be rebuilt from a fresh clone. **Not modified.** BSD 3-clause;
`ogg/COPYING`, `vorbis/COPYING` and `vorbis/AUTHORS` are the upstream files.

    https://downloads.xiph.org/releases/ogg/libogg-1.1.3.tar.gz
    https://downloads.xiph.org/releases/vorbis/libvorbis-1.2.0.tar.gz

## Why these exact releases

The game's audio middleware, FMOD Ex, vendored libogg/libvorbis under
`lib/ogg_vorbis/`, and the image names fourteen of those files in assert
strings. **The release is not stamped anywhere in the image** — `Xiph`,
`libVorbis` and `Vorbis I` all appear zero times — so it was decided by
compiling each candidate with the title's own `cl.exe` and counting how much
of the image the result identifies:

```
ogg            vorbis         indexable  found  at start  named
libogg-1.1.3   libvorbis-1.2.0      342     37        35  8 of 14
libogg-1.1.4   libvorbis-1.2.0      348     33        31  8 of 14
libogg-1.1.3   libvorbis-1.2.2      284     32        30  7 of 14
libogg-1.1.4   libvorbis-1.2.2      290     28        26  7 of 14
```

Both axes move independently and both point at the older pair, which is the
one contemporaneous with FMOD's other vendored codec — the image also carries
`reference libFLAC 1.2.1 20070917`. `FINDINGS.md` §8a has the full account.

## What is here and what is not

Everything needed to compile the files the image contains, and nothing else:

* **excluded** — `lib/books/` and `lib/modes/`, 980 KB of encoder codebook
  tables used only by `vorbisenc.c`, which this image does not contain;
* **excluded** — `barkmel.c`, `psytune.c`, `tone.c`, standalone test
  programs, and `vorbisenc.c` with them;
* **excluded** — autotools, documentation and the win32 project files.

## Building these

`src/manifest.txt` carries the flags per row, and they are not the flags the
rest of the project uses:

```
/O2 /Gy /GS- /fp:fast /D__BORLANDC__ /I<the three include dirs>
```

`/D__BORLANDC__` is **forced, not chosen**. `vorbis/lib/os.h` guards an x86
`__asm { fld f / fistp i }` implementation of `vorbis_ftoi` behind

```c
#if defined(_WIN32) && !defined(__GNUC__) && !defined(__BORLANDC__)
```

and the XDK compiler satisfies that test, so `vorbisfile.c` and `lsp.c` died
with `C2759: Unknown opcode: fld`. Defining `__BORLANDC__` falls through to
the portable `(int)(f+.5)`. It is surgical: `__BORLANDC__` appears exactly
once in either tree, on that line. Whether FMOD did the same or patched
`os.h` is not known.

## Six files still contribute nothing

`psy.c`, `res0.c`, `smallft.c`, `floor0.c`, `mapping0.c` and `envelope.c` are
named in the image and match nothing here. The release and the flags are both
eliminated by measurement, so what is left is FMOD's own changes to the tree
it vendored — which is what a vendored tree predicts, and is a hypothesis
rather than a conclusion.
