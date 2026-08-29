# Matched functions

Byte-for-byte matches against the retail image, compiled with the original
XDK 8276 toolchain (`cl.exe` 15.00.8153).

Verify any row with:

    python tools/match.py <source> <address>

| address | bytes | source | symbol | flags |
|---|---|---|---|---|
| `822607F0` | 120 | `src/grid_indices.cpp` | `BuildGridStripIndices` | `/O2 /Gy /GS- /fp:fast` |

**1 of 25,737 functions.** 18,126 are unattributed (not XDK, not identified
middleware) and are the real target; `python tools/candidates.py` narrows that
to 2,565 vetted leaf targets.

## In progress

`sub_827618E8` — 136 bytes, a counted wide-string compare, 3 callers.
`src/wstr_compare.cpp` reaches the right SIZE and the right instruction mix but
the wrong loop rotation: the target keeps the original counts in callee-saved
r30/r31 across the loop and rotates the exit test differently. Not matched.
