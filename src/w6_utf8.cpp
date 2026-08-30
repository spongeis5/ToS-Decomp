#include "types.h"

// sub_822CEC98 -- UTF-8 next-code-point: advances *s and returns the value.
// 188 B, 4 callers.
//
//   c < 0x80            one byte
//   0x80 <= c < 0xE0    two bytes,  (c&0x1F)<<6  | c2&0x3F
//   0xE0 <= c < 0xF0    three bytes -- MSVC builds it LEFT-LEANING:
//                       ((c&0xF)<<6 | c2&0x3F) << 6 | c3&0x3F, so the
//                       shifts compose through the adds (rlwinm 6,22,25
//                       then rlwinm 6,0,25 on the partial sum)
//   c >= 0xF0           four bytes, same progressive form
//
// The pointer store lands BEFORE the shift/add tail in each arm.

int Utf8Next(char** s)
{
    unsigned char* p = (unsigned char*)*s;
    unsigned int c = p[0];
    *s = (char*)(p + 1);
    if (c < 0x80)
        return c;
    if (c < 0xE0)
    {
        unsigned int c2 = p[1];
        *s = (char*)(p + 2);
        return ((c & 0x1F) << 6) | (c2 & 0x3F);
    }
    if (c < 0xF0)
    {
        unsigned int c2 = p[1];
        unsigned int c3 = p[2];
        *s = (char*)(p + 3);
        return ((((c & 0xF) << 6) | (c2 & 0x3F)) << 6) | (c3 & 0x3F);
    }
    unsigned int c2 = p[1];
    unsigned int c3 = p[2];
    unsigned int c4 = p[3];
    *s = (char*)(p + 4);
    return ((((c & 0x7) << 6) | (c2 & 0x3F)) << 6 | (c3 & 0x3F) << 6)
           | (c4 & 0x3F);
}

// NEAR-MISS. shift-tree associativity: image composes 12-bit shifts through two 6-bit adds; source spelling produces different tree.
