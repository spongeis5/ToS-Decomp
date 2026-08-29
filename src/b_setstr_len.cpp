// sub_82805F48 -- NULL-guarded assign that measures the string first.
// 52 bytes, 40 callers.
//
//      cmplwi  cr6,r4,0
//      bne-    cr6,measure
//      addi    r3,r3,24            NULL: hand back the embedded buffer
//      blr
//  measure:
//      mr      r11,r4
//  L:  lbz     r10,0(r11)
//      addi    r11,r11,1
//      cmplwi  cr6,r10,0
//      bne+    cr6,L
//      subf    r11,r4,r11
//      addi    r11,r11,-1          p - s - 1  == strlen
//      rotlwi  r5,r11,0            zero-extend to 64 bits
//      b       0x82805E58          tail call
//
// The NULL case is the FALL-THROUGH, so it is the first branch of the source
// `if`.
//
// The strlen is the COMPILER'S INTRINSIC, not a hand-written loop, and that
// is the whole reason this matches. An equivalent loop written out in the
// source --
//
//      const char* p = s;  while (*p++) ; return (int)(p - s - 1);
//
// -- produces the same seven instructions and then folds the -1 straight
// into the argument register: `addi r5,r11,-1`, 10 of 12 words. The target
// keeps the subtraction in r11 and follows it with `rotlwi r5,r11,0`, which
// is `rlwinm r5,r11,0,0,31`: an explicit 32-to-64 zero extension of the
// result. `strlen` returns size_t, and the intrinsic expansion materialises
// that unsigned width rather than assuming it. Declaring the CRT prototype
// and asking for the intrinsic reproduces it exactly.
//
// This is also a different strlen from the already-matched sub_82540728,
// which peels its first byte and walks with `lbzu`. That one is a real
// function in the image; this one is inlined code.

#include "types.h"

extern "C" size_t strlen(const char*);
#pragma intrinsic(strlen)

struct Object
{
    /* 0x00 */ char unk0000[0x18];
    /* 0x18 */ char buffer[1];
};

ASSERT_OFFSET(Object, buffer, 0x18);

char* AssignString(Object* o, const char* s, u32 len);

char* SetString(Object* o, const char* s)
{
    if (s == 0)
        return o->buffer;
    return AssignString(o, s, strlen(s));
}
