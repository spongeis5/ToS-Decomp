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
// NEAR MISS: 70 of 75 words at /O2, right size (300 bytes), and every word
// from 8215A5F8 to the end is identical. All five differences are in the
// ENTRY BLOCK and they are ONE TRANSPOSITION in the order values are created.
//
// READ IT AS A CREATION ORDER, because that is what decides the register
// numbers -- the same lever that finished sub_82600960:
//
//      target  32=r11, str=r10, map-1=r9, count=r3, longest=r31,
//              ctrl-1=r7, clear zero=r11 (recycled: mtctr r11 freed it)
//      ours    32=r11, str=r10, map-1=r9, count=r3, clear zero=r8,
//              ctrl-1=r7, longest=r31
//
// so `longest` and the clear loop's zero are in each other's slots, and the
// fresh r8 is a consequence: created fifth, the zero cannot have r11, which
// `mtctr` does not free until later. Nothing else in 300 bytes differs.
//
//      want  li r11,32 ; addi r9,r1,-49 ; mr r10,r3 ; li r3,0 ; li r31,0
//            mtctr r11 ; addi r7,r4,-1 ; li r11,0 ; stbu r11,1(r9) ; bdnz+
//      got   li r11,32 ; addi r9,r1,-49 ; mr r10,r3 ; li r3,0 ; li r8,0
//            mtctr r11 ; stbu r8,1(r9) ; bdnz+ ; addi r7,r4,-1 ; li r31,0
//
// THE ASSIGNMENT AHEAD OF THE CLEAR LOOP IS `count = 0`, NOT `longest = 0`.
// This file had `longest` there for a long time, at 69 of 75, on the strength
// of a sweep that varied one assignment at a time; the two are not
// interchangeable and `count` is worth one more word -- it puts `li r3,0` in
// the target's own slot at 8215A5DC. Found by sweeping all 65 (subset, order)
// placements of the four prologue assignments rather than one at a time.
//
// WHAT IS OPPOSED, and it is a genuine trade rather than a missing spelling.
// A SECOND assignment ahead of the clear loop does create `longest` fifth --
// 8215A5DC and 8215A5E0 both agree -- and swaps `str` and `map-1`, which
// renames twenty words downstream, for a net 49 of 75. Every construct that
// executes `longest = 0` before the loop lands on that same 47..49 plateau:
// `count = longest = 0`, the comma form, either order of two statements, the
// `for` loop's own init clause, the assignment sunk into the loop body, and a
// declaration initialiser. Every construct that executes it after the loop is
// 70. There is no third position.
//
// MEASURED AND RULED OUT -- roughly 160 compiles, in five sweeps:
//
//   all 65 subset/order placements of count, longest, ctrl, str
//                                            best 70 (count alone), then 69
//   the order of the assignments AFTER the loop, all six
//                                            byte-identical, no information
//   the clear loop as `for`, `while`, or `do/while` over a pointer
//                                            byte-identical, all three
//   seven declaration orders of the eight locals (map first, map last, str
//     first, i first, the counters first, ...) under four placements each
//                                            byte-identical, 28 shapes, a
//                                            clean negative on that axis
//   `longest` initialised in its declaration instead of assigned
//                                            same 47 as the assignment, so
//                                            the position is the whole of it
//   the clear loop's zero named in a local declared late      70, unchanged
//   the map build lifted into an inlined helper (both splits) 296 B, 49
//   the clear loop counting down                              300 B, 1
//   `ctrl = control - 1` with the increment written first     70, identical
//   `u8* m = map - 1; *++m = 0;`                 296 B, 3  (a memset call)
//   `for (count = 0; ; count++)` on the outer loop           268 B, 1
//   all 72 flag combinations from tools/flagsweep.py: 44 give this same
//     70-of-75 shape and 28 give 260 bytes at 8 of 75, so the level is /O2
//     and the flag axis is exhausted
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
    count = 0;

    for (i = 0; i < 32; i++)
        map[i] = 0;

    ctrl = control;
    longest = 0;
    str = string;

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
