// sub_8215A5C8 -- count the tokens in a string and report the longest.
// 300 bytes, 1 caller.
//
// It is 0x130 before src/y2_tokenize_into.cpp (sub_8215A6F8) and is the same
// routine with the copy replaced by a count: the identical 32-byte delimiter
// map, built by the identical `stbu` clear loop and `do/while` over the
// control string, and the identical skip loop. Two functions of one
// translation unit, and both are `/O2`.
//
//      li     r11,32 ; addi r9,r1,-49 ; mtctr r11 ; li r11,0
//      stbu   r11,1(r9) ; bdnz+          clear map[32] at r1-48
//      li     r6,1                       the shift base, held throughout
//      mr     r10,r3                     str = string, unconditionally
//      li     r3,0                       count, and the return register
//      li     r31,0                      longest
//
// The outer loop is a `for (;;)` with two exits, and which one a path takes
// is readable: `beq- cr6,8215A6DC` at 8215A680 leaves for the tail when the
// character after the skip is NUL, while `beq- cr6,8215A6DC` at 8215A6C4
// leaves on `start == str`. Both land on the same block because MSVC
// threaded the first through the second -- if the scan loop never runs,
// `str` is unchanged and the `start == str` test is known true.
//
// `cmplw cr6,r8,r31` on the length against the running maximum is UNSIGNED,
// so both are `u32`; the token count in r3 is incremented with `addi` and
// never compared, so its signedness is not readable and it is written `u32`
// to go with the other.
//
// The out-parameter is optional: `cmplwi cr6,r5,0 ; beq-` guards the single
// store, and the guard is written last because the store is the fall-through.
//
// NEAR MISS: 69 of 75 words at /O2, right size (300 bytes), and every word
// from 8215A5F8 to the end is identical. All six differences are in the
// ENTRY BLOCK, and they are one scheduling decision:
//
//      want  li r11,32 ; addi r9,r1,-49 ; mr r10,r3 ; li r3,0 ; li r31,0
//            mtctr r11 ; addi r7,r4,-1 ; li r11,0 ; stbu r11,1(r9) ; bdnz+
//      got   li r11,32 ; addi r9,r1,-49 ; mr r10,r3 ; li r31,0 ; li r8,0
//            mtctr r11 ; stbu r8,1(r9) ; bdnz+ ; li r3,0 ; addi r7,r4,-1
//
// -- retail hoists `count = 0` and the `ctrl - 1` induction base ABOVE the
// clear loop and reuses r11 for the clear value; we leave both below and
// take a fresh r8.
//
// WHAT WAS MEASURED, and it is the interesting part: putting those
// assignments before the clear loop in the source does NOT move them there,
// it BREAKS THE ADDRESS FOLD. With `str = string` (or three or four
// initialisers) ahead of the loop, MSVC stops emitting `addi r9,r1,-49` and
// emits `addi r9,r1,-48 ; addi r11,r9,-1` instead -- one word longer, 304
// bytes, and the whole function shifts. The two goals are in direct
// opposition along this axis, and 22 spellings were compiled to establish
// that:
//
//   nothing before the clear loop                    300 B, 66 of 75
//   `longest = 0` only before it                     300 B, 69 of 75  <- best
//   `count = 0` only                                 300 B, 48
//   `ctrl = control` only                            300 B, 47
//   count+longest / count+longest+ctrl / six orders  300 B, 49..52
//   any set including `str = string`                 304 B,  7  (fold broken)
//   clear loop over `u8* m = map` with inits first   304 B,  7
//   clear loop as a `do/while` with inits first      304 B,  7
//   `u8* m = map - 1; *++m = 0;`                     296 B,  3  (memset call)
//   `for (count = 0; ; count++)` on the outer loop   268 B,  1
//   `map` declared last; `u32` clear counter         304 B,  7
//   /O2 /Os                                          291 B,  6 of 66
//
// So this is NOT a declaration-order problem in the ordinary sense: the
// hoist retail performs is above a loop, and no source order tried reaches
// it without costing the fold. Left at the 69-of-75 shape.
//
// Its sibling sub_8215A6F8 matched 78 of 78 at /O2 with the same map and the
// same loops, so the difference is confined to these two assignments.

#include "types.h"

u32 CountTokens(const u8* string, const u8* control, u32* outLongest)
{
    u8 map[32];
    const u8* str;
    const u8* ctrl;
    const u8* start;
    u32 count;
    u32 longest;
    u32 len;
    int i;
    longest = 0;

    for (i = 0; i < 32; i++)
        map[i] = 0;

    ctrl = control;
    str = string;
    count = 0;

    do
    {
        map[*ctrl >> 3] |= (u8)(1 << (*ctrl & 7));
    }
    while (*ctrl++ != 0);

    for (;;)
    {
        while ((map[*str >> 3] & (1 << (*str & 7))) && *str != 0)
            str++;

        start = str;
        len = 0;

        while (*str != 0)
        {
            if (map[*str >> 3] & (1 << (*str & 7)))
            {
                str++;
                break;
            }
            str++;
            len++;
        }

        if (start == str)
            break;

        if (len > longest)
            longest = len;
        count++;
    }

    if (outLongest != 0)
        *outLongest = longest;

    return count;
}
