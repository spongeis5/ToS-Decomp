// sub_8215A420 -- case-folding string hash, h = h*131 + toupper(c).
// 64 B, 147 callers.
//
//   lbz    r11,0(r3)          c = *s
//   mr     r10,r3             the walking pointer moves out of r3 ...
//   li     r3,0               ... because r3 is the accumulator and the return
//   cmplwi cr6,r11,0          UNSIGNED test on the raw byte
//   beqlr  cr6                empty string -> 0
// L:rlwinm r7,r11,0,25,25     c & 0x40
//   mulli  r8,r3,131          h * 131
//   srawi  r6,r7,1            (c & 0x40) >> 1  == 0x20 when bit 6 is set
//   and    r5,r6,r11          & c              == 0x20 only for 'a'..'z'
//   subf   r4,r5,r11          c - that         -> uppercase
//   lbzu   r11,1(r10)         c = *++s
//   extsb  r9,r4
//   cmplwi cr6,r11,0
//   add    r3,r9,r8
//   bne+   cr6,L
//   blr
//
// The fold is branchless: bit 6 and bit 5 set together identify 'a'..'z', and
// subtracting 0x20 clears bit 5.
//
// THE SIGNEDNESS IS THE WHOLE FUNCTION. The character has to be UNSIGNED
// where the loop tests it and SIGNED inside the fold, and getting either one
// wrong changes the instruction count:
//
//   * all-unsigned (`u8 c`): the compiler proves `c & 0x40` non-negative and
//     collapses the mask and the shift into ONE `rlwinm r10,r10,31,26,26`.
//     That costs it a register -- the fold's destination is also its source,
//     so `c` has to be copied into it every iteration -- and the body comes
//     out 68 bytes with two `mr`s the target does not have.
//
//   * all-signed (`char c`): the loop test needs the sign-extended value, so
//     the compiler materialises `extsb` before `cmpwi` and keeps it live. The
//     inner five instructions then match the target EXACTLY, and the function
//     is 72 bytes because of the extra extsb per iteration.
//
// Written as it is here -- unsigned byte tested, signed char folded -- the
// mask is applied to the raw byte, the reassociation `(c & 0x40) >> 1` ->
// `(c >> 1) & 0x20` is not taken (it would require the sign extension the
// compiler otherwise avoids), and nothing is extended except the result.
// 16 words, all identical.
//
// The loop is rotated: the first character is loaded and tested before the
// body, with the bottom test `bne+` backwards. That is a `while`, not a
// do/while -- the guard returns 0 through the register the loop accumulates
// in.

#include "types.h"

int HashStringUpper(const u8* s)
{
    int h = 0;

    while (*s != 0)
    {
        char c = (char)*s;
        h = h * 131 + (char)(c - (((c & 0x40) >> 1) & c));
        ++s;
    }

    return h;
}
