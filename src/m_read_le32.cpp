#include "types.h"

// sub_825BD9B0 -- read a little-endian 32-bit value on a big-endian machine.
// 48 B, 8 callers.
//
//      lwz     r11,0(r3)
//      lbz     r10,17(r11) ; lbz r9,16(r11)
//      rotlwi  r8,r10,8
//      lbz     r7,15(r11)  ; lbz r6,14(r11)
//      or      r5,r8,r9
//      rlwinm  r4,r5,8,0,23 ; or r3,r4,r7
//      rlwinm  r11,r3,8,0,23 ; or r3,r11,r6
//      blr
//
// The bytes are taken at 17, 16, 15, 14 -- DESCENDING -- and shifted into a
// value most-significant first, so the result is
// `p[17]<<24 | p[16]<<16 | p[15]<<8 | p[14]`. That is a little-endian load
// done by hand, which is what a big-endian console does when it reads a
// format defined little-endian.
//
// Four separate `lbz`s and no `lwz`, so the source really does index bytes;
// a `lwz` plus a byte swap would be two instructions.
struct ByteSource
{
    const u8* data;
};

u32 ReadLE32At14(const ByteSource* s)
{
    const u8* p = s->data;
    u32 v = p[17];
    v = (v << 8) | p[16];
    v = (v << 8) | p[15];
    v = (v << 8) | p[14];
    return v;
}
