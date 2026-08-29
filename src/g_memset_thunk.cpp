#include "types.h"

// sub_82697740 -- the 68 bytes the inventory records here are NOT one
// function. The first 8 are a thunk,
//
//      82697740  b     0x828A8C50      (a hand-written CRT memset)
//      82697744  .long 0               COMDAT padding to 8
//
// with two more of exactly the same shape immediately before it (82697730 ->
// 0x828A8CF0, 82697738 -> 0x828A9AF0). The remaining 60 bytes are a separate
// counted byte compare, whose body starts at 82697748 -- see
// src/g_memcmp_n.cpp. Discovery sized the thunk as extent-to-next-known-start
// and swallowed the function behind it.
//
// This file is the thunk half. match.py PRINTS MATCH for it and exits 0, and
// that result is worthless -- read its own line:
//
//      1 word(s) compared: 0 identical, 0 differ, 1 differ in a relocated word
//
// Zero words were verified. can_shrink is happy because our single word is an
// unconditional terminator, the retail word there is one too, and clause (4)
// -- "every non-relocated word of the prefix agrees" -- is vacuously true over
// an empty set. Any source whose first instruction is a tail call would pass
// here identically. Do NOT put this row in src/manifest.txt on the strength of
// that MATCH; it is recorded only to say what the address holds.

extern "C" void* __cdecl memset(void* dst, int c, size_t n);

void* FillBytes(void* dst, int c, size_t n)
{
    return memset(dst, c, n);
}
