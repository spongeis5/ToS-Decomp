#include "types.h"

// sub_822020B0 -- three dependent loads. 16 B, 10 callers.
//   lwz r11,4(r3) ; lwz r10,4(r11) ; lwz r3,156(r10) ; blr
struct C3 { /* 0x9C */ char unk0000[0x9C]; void* v; };
struct C2 { /* 0x04 */ char unk0000[0x04]; C3*   n; };
struct C1 { /* 0x04 */ char unk0000[0x04]; C2*   n; };
ASSERT_OFFSET(C3, v, 0x9C);
ASSERT_OFFSET(C2, n, 0x04);
ASSERT_OFFSET(C1, n, 0x04);
void* Chain156(C1* p) { return p->n->n->v; }
