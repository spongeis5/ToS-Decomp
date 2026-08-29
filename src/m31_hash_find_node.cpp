// sub_8261B1A8 -- hash a C string, then walk a chain looking for the hash.
// 100 bytes, 4 callers.
//
//      lbz     r11,0(r4) ; mr r10,r4 ; li r8,0
//      extsb   r11,r11 ; cmpwi cr6,r11,0 ; beq- cr6,<after>
//  loop:
//      mulli   r8,r8,131
//      lbzu    r9,1(r10)
//      add     r8,r8,r11
//      extsb   r11,r9 ; cmpwi cr6,r11,0 ; bne+ cr6,loop
//  after:
//      lwz     r3,72(r3)
//      lwz     r11,0(r3) ; cmpwi cr6,r11,0 ; beq- cr6,<zero>
//  find:
//      lwz     r10,8(r3) ; cmplw cr6,r10,r8 ; beqlr cr6
//      mr      r3,r11 ; lwz r11,0(r11) ; cmpwi cr6,r11,0 ; bne+ cr6,find
//  zero:
//      li      r3,0 ; blr
//
// `extsb` plus a SIGNED `cmpwi` on every character: plain `char`, which is
// signed for this compiler. (Contrast sub_8215A420, where the loop test is
// `cmplwi` on the raw byte and only the fold is signed -- there the split
// mattered; here both halves are signed and one `const char*` gives it.)
//
// The test is peeled out in front of the loop and repeated at the bottom,
// which is MSVC's rotation of a `while`, not a `do/while` (a do/while has no
// peeled copy).
//
// `cmpwi cr6,r11,0` on the LINK, which is then dereferenced, is the signed-
// compare tell from sub_82631D98: every pointer null test in this image is
// `cmplwi`, so a signed one means the field is an `int` holding a pointer.
//
// The link is loaded before the body runs and reused as the advance, so the
// loop condition is the NEXT node -- `for (p = head; p->next; p = p->next)`,
// which does not test the last node and is what the image does.
//
// `cmplw cr6,r10,r8` puts the node's field in rA: written `p->key == h`.
//
// `mr r10,r4` is the whole reason the walk needs a SEPARATE local. Advancing
// the parameter itself -- `while (*s) { ...; s++; }` -- lets MSVC run `lbzu`
// straight off r4 and the copy disappears, which is 96 bytes against the
// image's 100 and 1 of 24 words, every register renamed downstream. The
// pointless-looking move is the tell that the source did not consume the
// parameter.
//
// Nothing is relocated: 25 of 25 words are compared.

#include "types.h"

struct HashNode
{
    /* 0x00 */ s32 next;
    /* 0x04 */ u8  unk0004[4];
    /* 0x08 */ u32 key;
};

ASSERT_OFFSET(HashNode, key, 0x08);

struct NameTable
{
    /* 0x00 */ u8        unk0000[0x48];
    /* 0x48 */ HashNode* chain;
};

ASSERT_OFFSET(NameTable, chain, 0x48);

HashNode* FindByName(NameTable* t, const char* s)
{
    u32 h = 0;
    const char* c = s;

    while (*c != 0)
    {
        h = h * 131 + *c;
        c++;
    }

    for (HashNode* p = t->chain; p->next != 0; p = (HashNode*)p->next)
    {
        if (p->key == h)
            return p;
    }

    return 0;
}
