// sub_825FAC00 -- copy two words in, remember the node, and then pack three
// signed bytes out of that node into three 3-bit slots of one field.
// 112 B, 5 callers.
//
//   lwz r11,0(r5) ; stw r11,4(r3) ; lwz r10,4(r5) ; stw r10,8(r3)
//   stw r4,0(r3)  ; beqlr cr6
//
// Store order is 4, 8, 0, which is source order. The `cmplwi cr6,r4,0` is
// hoisted to the second instruction but its `beqlr` is after all three
// stores, so the guard is written after the copy.
//
// The three packs are ordinary read-modify-write, NOT bitfields: MSVC emits
// `rlwimi` for a bitfield store (MATCHED.md), and there is none here. Each
// `rlwinm` decodes as a plain shift or a plain mask --
//
//   rlwinm r8,r10,0,0,28   = & ~7        rlwinm r10,r11,3,0,28  = << 3
//   rlwinm r9,r6,0,29,25   = & ~0x38     rlwinm r4,r5,6,0,25    = << 6
//   rlwinm r11,r7,0,26,22  = & ~0x1C0
//
// and the value side is NOT masked to the slot, so the source trusts the byte
// to be in range rather than clamping it.
//
// `lbz` + `extsb` with no cast in sight is how MSVC loads a `signed char`, so
// the three node fields are `s8`.
//
// `rotlwi r6,r7,0` and `rotlwi r7,r8,0` are two words that NO ordinary
// spelling of the three assignments produces. Twelve shapes were measured
// and eleven are the same 104 B, 16 of 26 -- field-first, value-first, an
// `int*` local, an `int&` reference, an inlined helper RETURNING the packed
// value, a temp per statement, positive hex masks, a `u32` field, the member
// form, and the shift hoisted into its own local. All eleven forward the
// stored register straight into the next mask and lose both copies.
//
// Only an inlined helper taking the field's ADDRESS reaches 28 of 28:
//
//     static void Pack(int* p, int mask, int v) { *p = (*p & mask) | v; }
//
// That is MATCHED.md's address-of lever, and note that the bare `int* p`
// local is NOT enough here -- the pointer has to cross a call boundary. It
// also puts the mask AFTER the byte load, where every other shape computes
// it first straight out of the store's own register.
//
// The FIRST read after the entry block is a real reload (`lwz r10,4(r3)`)
// because `beqlr` ends that block, and every shape gets that part right.

#include "types.h"

struct Node
{
    /* 0x00 */ u8 unk0000[25];
    /* 0x19 */ s8 a;
    /* 0x1A */ s8 b;
    /* 0x1B */ s8 c;
};

ASSERT_OFFSET(Node, a, 25);
ASSERT_OFFSET(Node, b, 26);
ASSERT_OFFSET(Node, c, 27);

struct Slot
{
    /* 0x00 */ Node* node;
    /* 0x04 */ int   bits;
    /* 0x08 */ u32   extra;
};

ASSERT_OFFSET(Slot, bits, 4);
ASSERT_OFFSET(Slot, extra, 8);

static void Pack(int* p, int mask, int v)
{
    *p = (*p & mask) | v;
}

void SlotSet(Slot* o, Node* n, const u32* v)
{
    o->bits  = (int)v[0];
    o->extra = v[1];
    o->node  = n;

    if (n == 0)
        return;

    Pack(&o->bits, ~7,     n->a);
    Pack(&o->bits, ~0x38,  n->b << 3);
    Pack(&o->bits, ~0x1C0, n->c << 6);
}
