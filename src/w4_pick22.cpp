#include "types.h"

// sub_82710230 -- pick a node: the override at +792 when present, else the
// embedded one at +48; then tail-call one of two functions on the kind.
// 48 B, 4 callers.
//
//      lwz     r11,792(r3)      the override
//      cmpwi   cr6,r5,22        the kind test, SIGNED
//      addi    r3,r3,48         the embedded node's address
//      bgt-    cr6,0x82710250   kind > 22 -> second tail
//      cmplwi  cr6,r11,0 ; beq- ; mr r3,r11
//      b       82706338
//      cmplwi  cr6,r11,0 ; beq- ; mr r3,r11
//      b       82706410
//
// The null-check and `mr` appear ONCE PER ARM because the ternary is inside
// each arm -- spelling it once above the branch folds into r3 with no
// reload. addi r3,r3,48 is emitted ahead of the branch because it feeds both
// arms; the ternary then overrides r3.

struct Pickler
{
    /* 0x30 */ char  unk0000[48];
    /* 0x30 */ char  embedded[744];
    /* 0x318 */ void* override;
};

ASSERT_SIZE(Pickler, 796);   /* 32-bit pointers: 48 + 744 + 4 */
ASSERT_OFFSET(Pickler, override, 792);

void* Tail_82706338(void*);
void* Tail_82706410(void*);

void* Pick(Pickler* p, int pad, int kind)
{
    void* o = p->override;
    if (kind <= 22)
        return Tail_82706338(o ? o : &p->embedded);
    return Tail_82706410(o ? o : &p->embedded);
}

// NEAR-MISS. Schedule of the ternary select against the kind branch: the
// image loads the override, tests kind, hoists the false value (addi
// r3,r3,48), then selects per arm. Inline-per-arm, named-local and
// hoisted-load spellings each give a different order; none gives this one.
