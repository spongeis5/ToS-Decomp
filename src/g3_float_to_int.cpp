// sub_826306F8 -- float to int, truncating toward zero, done ENTIRELY in
// integer ops on the float's bit pattern. 144 B, 5 callers.  f1 in, r3 out.
//
// The whole function decodes; what it computes is not in doubt.  Writing
// `b` for the bit pattern and `E` for the unbiased exponent:
//
//   stfs f1,-16(r1) ; lwz r9,-16(r1)     reinterpret the float as an int
//   rlwinm r11,r9,9,24,31 ; addi -127    E = ((b >> 23) & 0xFF) - 127
//   clrlwi r10,r9,1 ; addi -1 ; srawi 31 Z  = -1 when |x| == 0
//   andc  r9,r9,r5                       B  = Z ? 0 : b
//   addi r4,r8,-1 ; srawi r10,r4,31      M1 = -1 when E < 24
//   subfic r3,r11,23 ; and r4,r3,r10     SH1 = M1 ? (23 - E) : 0
//   clrlwi r3,r10,27 ; rlwinm r3,r3,0,29,27
//                                        M1 & 23 -- TWO rlwinms because 23
//                                        is 10111 and a single rlwinm mask
//                                        must be contiguous; `andi.` would
//                                        do it in one but clobbers CR0
//   subf r5,r4,r3                        SH3 = (M1 & 23) - SH1  = M1 ? E : 0
//   sraw r10,r7,r5 ; or r5,r10,r3        MSK = (0xFF800000 >>a SH3) | ~M1
//   rlwimi r7,r6,23,0,8                  SIG = (B & 0x7FFFFF) | 0x800000
//   and r9,r7,r5                         W   = SIG & MSK   -- truncate the
//                                        fraction bits, so that the later
//                                        arithmetic shift of a NEGATIVE
//                                        value truncates instead of flooring
//   rlwinm r11,r9,1,0,30 ; addi -1 ; or ; subf ; addi 1
//                                        R   = SGN ? -W : W, branchless
//   andc r10,r11,r7                      zero when E < 0, i.e. |x| < 1
//   sraw r9,r10,r4 ; slw r3,r9,r6        (R >> SH1) << ((E - 23) & ~M1)
//
// So this is `(int)f` written by hand rather than through `fctiwz`, and the
// result is exact truncation toward zero for every finite input.
//
// MEASURED, NOT MATCHED: 1 of 36 words, 148 bytes against 144.  Every
// instruction the target uses appears in our output and no others, but the
// order and the register assignment are almost entirely different, because
// this transcription is the DATAFLOW read back out of the listing and not
// the expression tree the author wrote.  Two of its sub-expressions are
// certainly not what was written -- `(m1 & 23)` is spelled here as itself
// but the target builds it as `& 31` then `& ~8`, and the sign application
// `(((w << 1) - 1) | sgn) - w + 1` is an algebraic form no one writes by
// hand -- so the remaining distance is a search over expression trees with
// no lever pointing into it.
//
// The useful next step is not more shapes.  It is the routine's NAME: this
// is the standard soft-float float-to-int body, so if the archive it came
// from can be identified, `tools/libmatch.py` settles it byte-for-byte
// instead of anyone guessing at how the C was spelled.  Note also that the
// function is UNATTRIBUTED but need not be the title's own -- 144 bytes of
// pure bit manipulation with five callers is exactly the shape of a
// middleware or CRT helper.

#include "types.h"

s32 FloatToInt(f32 f)
{
    s32 b = *(s32*)&f;
    s32 e = ((b >> 23) & 0xFF) - 127;
    s32 z = ((b & 0x7FFFFFFF) - 1) >> 31;
    s32 v = b & ~z;
    s32 m1 = (e - 24) >> 31;
    s32 sh1 = (23 - e) & m1;
    s32 sh3 = (m1 & 23) - sh1;
    s32 msk = ((s32)0xFF800000 >> sh3) | ~m1;
    s32 w = (((v & 0x007FFFFF) | 0x00800000)) & msk;
    s32 sgn = v >> 31;
    s32 r = ((((w << 1) - 1) | sgn) - w) + 1;
    r = r & ~(e >> 31);
    return (r >> sh1) << ((e - 23) & ~m1);
}
