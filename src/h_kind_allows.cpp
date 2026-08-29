#include "types.h"

// sub_82255408 -- "is this object idle and of a kind outside 1..6". 144 B,
// 19 callers.
//
//      lwz     r10,8(r3)          o = h->obj
//      lwz     r11,2264(r10)      o->x
//      cmpwi   cr6,r11,0
//      bne-    cr6,T1
//      lwz     r11,2268(r10)      o->y
//      cmpwi   cr6,r11,0
//      li      r11,0
//      beq-    cr6,S1
//  T1: li      r11,1
//  S1: clrlwi  r11,r11,24         busy  -- MATERIALISED as a byte
//      cmplwi  cr6,r11,0
//      bne-    cr6,ret0
//      lwz     r10,2240(r10)      k = o->kind
//      cmpwi   cr6,r10,2 ; beq- T2
//      cmpwi   cr6,r10,3 ; beq- T2
//      cmpwi   cr6,r10,4 ; beq- T2
//      cmpwi   cr6,r10,5 ; bne- S2
//  T2: li      r11,1
//  S2: clrlwi  r11,r11,24         inner  -- MATERIALISED as a byte
//      cmplwi  cr6,r11,0
//      bne-    cr6,T3
//      cmpwi   cr6,r10,1 ; beq- T3
//      cmpwi   cr6,r10,6 ; bne- S3
//  T3: li      r11,1
//  S3: clrlwi  r11,r11,24         outer
//      li      r3,1
//      cmplwi  cr6,r11,0
//      beqlr   cr6
// ret0:li      r3,0
//      blr
//
// Three `clrlwi rX,rX,24` are three BOOL VALUES being produced, not three
// branches: a plain `if (a || b)` branches straight to the target and never
// materialises anything. Each one is an inlined bool-returning predicate.
//
// The nesting is readable too. The second predicate's false value is not a
// fresh `li r11,0` -- it reuses the zero already in r11 from the first, and
// the third's `||` chain starts from the second's byte. So the third is
// `inner || k == 1 || k == 6` with `inner` as a named first term, which is
// what an inlined bool function used inside another one's `||` looks like.

struct KindObj
{
    /* 0x000 */ char unk0000[0x8C0];
    /* 0x8C0 */ s32  kind;
    /* 0x8C4 */ char unk08C4[0x14];
    /* 0x8D8 */ s32  x;
    /* 0x8DC */ s32  y;
};
ASSERT_OFFSET(KindObj, kind, 0x8C0);
ASSERT_OFFSET(KindObj, x,    0x8D8);
ASSERT_OFFSET(KindObj, y,    0x8DC);

struct KindHolder
{
    /* 0x00 */ char     unk0000[0x08];
    /* 0x08 */ KindObj* obj;
};
ASSERT_OFFSET(KindHolder, obj, 0x08);

static bool IsBusy(const KindObj* o)
{
    return o->x != 0 || o->y != 0;
}

static bool IsInnerKind(s32 k)
{
    return k == 2 || k == 3 || k == 4 || k == 5;
}

static bool IsOuterKind(s32 k)
{
    return IsInnerKind(k) || k == 1 || k == 6;
}

int KindAllows(KindHolder* h)
{
    KindObj* o = h->obj;

    if (!IsBusy(o) && !IsOuterKind(o->kind))
        return 1;
    return 0;
}
