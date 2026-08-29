#include "types.h"

// sub_82202D08 -- pick one of two adjacent sub-objects, or fall back. 48 B.
//
//      lbz     r11,526(r3)      +0x20E
//      cmplwi  cr6,r11,0
//      beq-    cr6,other
//      lbz     r10,524(r3)      +0x20C
//      addi    r11,r3,484       &this->a   (+0x1E4)
//      cmplwi  cr6,r10,0
//      beq-    cr6,out
//      addi    r11,r11,20       ++p
// out: mr      r3,r11
//      blr
// other:lwz    r3,168(r3)
//      blr
//
// `addi r11,r11,20` adds to the pointer ALREADY in r11 rather than computing
// `this + 504` outright, so the source advanced a pointer it had -- `++p` on
// a 20-byte type -- and did not name the second sub-object. The stride is
// what fixes the size at 20; the two flags sitting at 0x20C and 0x20E, right
// after 0x1F8 + 20 = 0x20C, confirm the two slots are adjacent.
//
// BRANCH POLARITY WAS WRONG HERE AND IS NOW RIGHT: 6 of 12 words at the
// exact 48 bytes, up from 2 of 11 at 44. The fallback load is the target's
// OUT-OF-LINE block, so the enabled path is the source's fall-through and
// must be written first -- MATCHED.md, "Branch polarity is source order".
// With `if (!c->enabled) return c->fallback;` first, MSVC lays the fallback
// down as the fall-through and the whole function inverts. Four words at the
// top and bottom -- both flag reads' block layout, the cold `lwz r3,168(r3)`
// and both `blr`s -- come out right once the positive path leads.
//
// WHAT IS LEFT IS THE JOIN, and it is the register allocator:
//
//      want  lbz r10,524 ; addi r11,r3,484 ; cmplwi ; beq- out
//            addi r11,r11,20 ; out: mr r3,r11 ; blr
//      got   lbz r11,524 ; addi r3,r3,484  ; cmplwi ; beqlr
//            addi r3,r3,20 ; blr
//
// The target keeps the pointer in a SCRATCH and copies it to r3 at a real
// join; every shape that names the pointer lets MSVC coalesce it into r3,
// which then merges the inner exit into `beqlr` and loses the copy -- 44
// bytes, four short. TWENTY-FIVE shapes were compiled at both levels and
// they fall into exactly two groups:
//
//   48 B, 6 of 12 -- the two shapes that spell the second slot as its own
//     expression: `if (useSecond) return &slots[1]; return &slots[0];` and
//     the same as a ternary. Right size, but two independent `addi`s from
//     r3 instead of the target's derived `addi r11,r11,20`.
//   44 B, 2 of 11 -- everything that advances a NAMED pointer: `++p`,
//     `p += 1`, a `char*` advance, a goto join, two returns of `p`, a
//     one-variable if/else, the cold block written last, the flag in an
//     `int` local, an `Adv(base, flag)` helper, a one-level and a two-level
//     inlined helper, the member form, and the pointer named before the
//     guard (which is worse still, 1 of 12).
//
// So the source shape that produces `addi r11,r11,20` is the named pointer,
// and the source shape that produces the 48-byte layout is the unnamed one,
// and no spelling tried produces both. What the target needs is for `c` to
// still be live in r3 at the join; nothing in this function's source keeps
// it there, and MSVC's coalescer takes r3 whenever it is free.
struct Slot
{
    char unk0000[20];
};
ASSERT_SIZE(Slot, 20);

struct Chooser
{
    char  unk0000[0xA8];
    Slot* fallback;
    char  unk00AC[0x1E4 - 0xAC];
    Slot  slots[2];
    u8    useSecond;
    u8    unk020D;
    u8    enabled;
};
ASSERT_OFFSET(Chooser, fallback, 0xA8);
ASSERT_OFFSET(Chooser, slots, 0x1E4);
ASSERT_OFFSET(Chooser, useSecond, 0x20C);
ASSERT_OFFSET(Chooser, enabled, 0x20E);

Slot* PickSlot(Chooser* c)
{
    if (c->enabled)
    {
        if (c->useSecond)
            return &c->slots[1];
        return &c->slots[0];
    }
    return c->fallback;
}
