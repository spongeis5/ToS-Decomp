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
//
// FIVE MORE SHAPES, aimed specifically at the `mr r3,r11` -- that copy is
// MATCHED.md's un-naming signature, "the source repeated the expression, it
// CSEd into r11, and r3 was materialised with a copy" (sub_8224E178). None
// of them produces it:
//
//   48 B, 6 of 12 -- `return c->slots + 1;` / `return c->slots;`, the array
//     name decayed rather than `&c->slots[1]`. MSVC constant-folds 484 + 20
//     before CSE and emits two independent `addi r3,r3,504` / `addi
//     r3,r3,484`, exactly as the `&c->slots[1]` spelling does. The fold is
//     why no unnamed spelling can produce the derived `addi r11,r11,20`.
//   44 B, 2 of 11 -- a single `return p` through an if/else that also
//     assigns `p = c->fallback` in the else. MSVC still tail-duplicates the
//     cold return and still coalesces p into r3.
//   44 B, 2 of 11 -- the flag read through a named `const Chooser* v = c;`,
//     MATCHED.md's const-view CSE-tie lever. No change at all.
//   48 B, 2 of 12 -- `Slot* p = c->slots;` before the guard with the
//     POSITIVE polarity kept (the earlier note's "before the guard" was the
//     inverted one). This is the informative failure: MSVC hoists `addi
//     r3,r3,484` above the `enabled` test and moves `c` into r11, so the
//     cold block becomes `lwz r3,168(r11)`. It will put the POINTER in r3
//     and displace the parameter, which is the opposite of the target.
//   48 B, 2 of 12 -- `c->slots` repeated on both arms so it becomes a CSE
//     representative. Same hoist, and the increment still folds:
//     `addi r3,r11,504`.
//
// And /O2 /Os on the 48-byte shape is 4 of 12: it drops to cr0 (`cmplwi
// r11,0` / `beq- ` with no CR field), so the level is not the answer either.
//
// The one reading that fits everything seen: MSVC gives r3 to whichever of
// `c` and the result is live longer, and in the target `c` wins because the
// cold `lwz r3,168(r3)` is laid out AFTER the join. Every spelling that
// names the pointer makes the pointer win instead.
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
