// sub_8215A6F8 -- `strtok` that COPIES the token into a caller-supplied
// buffer instead of writing a NUL into the source. 312 bytes, 2 callers.
//
// It is the CRT's strtok body, structure for structure, with two changes:
// the saved position lives in the third argument rather than in a static,
// and the token is copied out.
//
//      li     r11,32 ; addi r9,r1,-49 ; mtctr r11 ; li r11,0
//      stbu   r11,1(r9) ; bdnz+                    <- clear the 32-byte map
//      li     r6,1                                 <- ONE `1`, held for the
//                                                     whole function
//   B: lbzu   r11,1(r7)                            <- do/while over ctrl,
//      rlwinm r8,r11,29,3,31 ; clrlwi r4,r11,29       so the NUL's own bit
//      slw    r4,r6,r4 ; lbzx r30,r8,r9               is set as well
//      or     r11,r4,r30 ; stbx r11,r8,r9
//      cmplwi cr6,r11,0 (issued early) ; bne+ B
//
// The map is 32 bytes at r1-48 and the two saved registers are at r1-16 and
// r1-8, below r1 with no `stwu`, so this is a frameless leaf.
//
// No `extsb` anywhere, so every character is UNSIGNED -- both string
// parameters are `const u8*`, which is what the CRT does internally too.
//
// The skip loop is the CRT's `while (in(map,*s) && *s) s++;` -- the map test
// FIRST and the NUL test second, which is visible as the pair of branches at
// 8215A784 and 8215A78C going to the same place in that order.
//
// The scan loop tests `*s != 0` at the top and the delimiter inside, with
// `addi r10,r10,1` on the delimiter path only, which is the CRT's
// `*string++ = 0; break;` turned into "step over the delimiter".
//
// The return is branchless: `subfic r8,r11,0 ; subfe r6,r7,r7 ; and r3,r6,r31`
// selects the buffer or zero on `s - start != 0`, the idiom MATCHED.md
// records with the mask carrying a value instead of 1.
//
// A SEPARATE WORKING LOCAL FOR THE STRING PARAMETER IS WHAT PUTS `out` IN
// r3, and that was the last four bytes. Written with the parameter walked
// directly --
//
//      char* out = ctx->buf;
//      if (s == 0) s = ctx->pos;
//      ... s++ ...
//
// -- MSVC leaves `s` in r3 and gives the output pointer r6: every
// instruction is right, in the right order, and 308 bytes against 312,
// because there is no `mr r10,r3`. Declaring
//
//      const u8* str;
//      if (string != 0) str = string; else str = ctx->pos;
//
// and walking `str` gives the parameter its own live range, which the
// allocator then moves out of r3 so the long-lived output pointer can have
// it: 78 of 78. Naming `const u8* ctrl = control;` the same way is the
// CRT's own shape and is kept for the same reason.
//
// So a `mr` of an incoming parameter into a scratch register at the TOP of
// a function -- before anything that could need it -- is evidence that the
// source copied the parameter into a local, not that the parameter is used
// early. `/O2 /Os` is 260 bytes and 5 of 65 here, so the level is not it.

#include "types.h"

struct TokCtx
{
    /* 0x00 */ const u8* pos;
    /* 0x04 */ char      buf[1];
};
ASSERT_OFFSET(TokCtx, buf, 0x04);

char* TokenizeInto(const u8* string, const u8* control, TokCtx* ctx)
{
    u8 map[32];
    char* out = ctx->buf;
    const u8* ctrl = control;
    const u8* str;
    const u8* start;
    int count;

    for (count = 0; count < 32; count++)
        map[count] = 0;

    do
    {
        map[*ctrl >> 3] |= (u8)(1 << (*ctrl & 7));
    }
    while (*ctrl++ != 0);

    if (string != 0)
        str = string;
    else
        str = ctx->pos;

    while ((map[*str >> 3] & (1 << (*str & 7))) && *str != 0)
        str++;

    start = str;

    while (*str != 0)
    {
        if (map[*str >> 3] & (1 << (*str & 7)))
        {
            str++;
            break;
        }
        *out++ = (char)*str++;
    }

    *out = 0;
    ctx->pos = str;
    return (str - start) != 0 ? ctx->buf : 0;
}
