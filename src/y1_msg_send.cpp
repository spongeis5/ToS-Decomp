#include "types.h"

// sub_82712E28 -- build a 14-byte record on the stack and hand its address
// to virtual slot 27. 88 B.
// Bridge between 82712E10 and 82712E80.
//
//      li r11,0 ; lwz r10,0(r3) ; li r9,1 ; li r8,-1
//      stw r11,84(r1) ; sth r11,88(r1) ; addi r4,r1,80
//      stb r11,90(r1) ; stb r11,92(r1) ; stb r11,93(r1)
//      stw r9,80(r1)  ; stb r8,91(r1)
//      lwz r11,108(r10) ; mtctr ; bctrl
//
// The record starts at r1+80, so the fields are +0 = 1, +4 = 0, +8 = 0 (a
// halfword), +10 = 0, +11 = -1, +12 = 0, +13 = 0.
//
// The emitted store order IS source order: the five zeroed fields in field
// order, then the 1, then the -1. Writing the record in plain field order
// instead puts the `stw r9,80(r1)` second and moves four other stores, at
// 13 of 22 -- MSVC does not group these by operand readiness.
//
// `li r8,-1` and not `li r8,255`, so the -1 field is SIGNED; `0xFF` in a u8
// gives the other immediate and nothing else changes.
//
// /O2 /Os. At plain /O2 everything is right except that the vtable slot load
// gets a fresh r7 where the image reuses r11, the register the zero was in
// -- MATCHED.md's register-coalescing signature, two words.

struct Sender;

struct SenderVT
{
    void* slot[27];
    void (*send)(Sender*, void*);
};

struct Sender
{
    /* 0x00 */ SenderVT* vt;
};
ASSERT_OFFSET(Sender, vt, 0x00);

struct SendRec
{
    /* 0x00 */ s32 a;
    /* 0x04 */ s32 b;
    /* 0x08 */ u16 c;
    /* 0x0A */ u8  d;
    /* 0x0B */ s8  e;
    /* 0x0C */ u8  f;
    /* 0x0D */ u8  g;
};
ASSERT_OFFSET(SendRec, c, 0x08);
ASSERT_OFFSET(SendRec, e, 0x0B);
ASSERT_OFFSET(SendRec, g, 0x0D);

void SendDefault(Sender* s)
{
    SendRec r;

    r.b = 0;
    r.c = 0;
    r.d = 0;
    r.f = 0;
    r.g = 0;
    r.a = 1;
    r.e = -1;

    s->vt->send(s, &r);
}
