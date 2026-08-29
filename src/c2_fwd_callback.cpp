// sub_8215F268 -- forward to a six-argument dispatcher, inserting the ADDRESS
// OF A FUNCTION as the second argument and two literals in the middle.
// 28 bytes, 5 callers.
//
//      lis     r11,-32157
//      mr      r8,r5
//      mr      r5,r4
//      li      r7,0
//      li      r6,1
//      addi    r4,r11,21064        = 82635248
//      b       0x8215D678
//
// r3 is never touched, so the first argument passes straight through.
//
// 82635248 is a REAL FUNCTION -- it is a `.pdata` row of 152 bytes and
// disassembles as `mflr r12 / stw r12,-8(r1) / std r31,-16(r1) /
// stwu r1,-112(r1) / mr r31,r3 / lwz r3,8(r3) ...`, storing r4, r5 and r6
// as BYTES into a stack block (`stb r4,88(r1)`, `stb r5,89(r1)`,
// `stb r6,90(r1)`). So the second argument here is a callback pointer, not
// a global object, and its three trailing parameters are byte-wide.
//
// lis+addi forming an address is the "address OF a global" form; a global
// POINTER variable would be lis+lwz (compare src/b_fwd_global5.cpp).
//
// The lis/addi pair and the tail branch are relocations, so 4 of the 7 words
// are compared.

#include "types.h"

struct Session;
struct Node;

void OnEvent(Node* n, u8 a, u8 b, u8 c);

typedef void (*EventFn)(Node*, u8, u8, u8);

int Dispatch(Session* s, EventFn fn, int a, int b, int c, int d);

int Post(Session* s, int a, int d)
{
    return Dispatch(s, OnEvent, a, 1, 0, d);
}
