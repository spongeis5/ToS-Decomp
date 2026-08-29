#include "types.h"

// sub_8215A460 -- the bounded twin of sub_8215A420 (c_hash_upper.cpp):
// case-folding string hash, h = h*131 + toupper(c), over at most n bytes.
// 80 B, 7 callers.
//
// It sits IMMEDIATELY after c_hash_upper (8215A420 + 0x40 = 8215A460), so
// the two are the same translation unit; c_hash_upper is /O2, and so is this.
//
//   mr     r9,r3              s out of r3 ...
//   li     r3,0               ... because r3 is the accumulator AND the result
//   li     r10,0              i = 0
//   cmplwi cr6,r4,0           the PEELED copy of `i < n` -- so it is a `for`,
//   beqlr  cr6                not a do/while
// L:lbz    r11,0(r9)          c = *s      (plain lbz + addi, not lbzu: the
//   cmplwi cr6,r11,0                       pointer is not biased here because
//   beqlr  cr6                             the index is what the loop tests)
//   rlwinm r6,r11,0,25,25     c & 0x40
//   mulli  r8,r3,131          h * 131
//   srawi  r5,r6,1            >> 1
//   addi   r10,r10,1          ++i
//   and    r3,r5,r11          & c    -- 0x20 only for 'a'..'z'
//   addi   r9,r9,1            ++s
//   subf   r11,r3,r11         c - that
//   cmplw  cr6,r10,r4         UNSIGNED, so i and n are unsigned
//   extsb  r11,r11
//   add    r3,r11,r8
//   blt+   cr6,L
//   blr
//
// Same signedness split as c_hash_upper and for the same reason: the byte is
// tested UNSIGNED (cmplwi on the raw load) and folded SIGNED (extsb on the
// result only). See c_hash_upper.cpp for the measurement -- all-unsigned
// collapses the mask and the shift into one rlwinm, all-signed adds an extsb
// to the loop test.
//
// The `*s == 0` exit is its own `beqlr` and the loop-count exit is the final
// `blr`: two returns of the same accumulator, not one shared tail.

int HashStringUpperN(const u8* s, u32 n)
{
    int h = 0;

    for (u32 i = 0; i < n; ++i)
    {
        if (*s == 0)
            return h;

        char c = (char)*s;
        h = h * 131 + (char)(c - (((c & 0x40) >> 1) & c));
        ++s;
    }

    return h;
}
