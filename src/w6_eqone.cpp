#include "types.h"

// sub_825FF490 -- is the byte at +3 equal to 1? 20 B, bridge between
// 825FF468 and 825FF4A8.
//
//      lbz     r11,3(r3)
//      addi    r11,r11,-1
//      cntlzw  r10,r11
//      rlwinm  r3,r10,27,31,31
//
// `addi -1 ; cntlzw ; rlwinm 27,31,31` is the branchless x == 1 idiom
// (idiom table). A bool return normalises, hence the final mask.

struct Byte3
{
    /* 0x03 */ char  unk0000[3];
    /* 0x03 */ unsigned char f3;
};

bool IsOne(Byte3* b)
{
    return b->f3 == 1;
}
