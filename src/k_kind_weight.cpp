#include "types.h"

// sub_827261D8 -- NOT MATCHED. 48 of 53 non-relocated words; the five that
// differ are the register allocation of ONE case body and nothing else. Kept
// for the shape, which is right, and for what it establishes about the tools.
//
// A 31-case switch on a kind byte that scales a table entry by a repeat
// count. 11 callers. The .pdata row records 60 bytes, which is the distance
// to the JUMP TABLE at 82726214; the body runs on past it to the epilogue at
// 82726340, 364 bytes in all -- and our object is 364 bytes with the table in
// the same place. Compare it with
//
//     python tools/match.py src/k_kind_weight.cpp 827261D8 --sym Weight
//
// -- `--sym` is required because the inline `Frames()` is emitted as its own
// COMDAT as well as being inlined, so the object holds two functions.
//
// A NOTE THAT WAS HERE AND IS NOW WRONG, kept because the correction is the
// point. This file used to say "MATCH.PY CANNOT SPEAK ABOUT THIS FUNCTION,
// whatever the source", because the inventory lists the jump table at
// 82726214 as a function start and can_extend's bound was therefore 60 bytes
// for a 364-byte body. tools/switches.py now supplies the jump-table ranges
// and match.py excludes them, so it compares all 91 words and would exit 0 on
// a correct source. A recorded tool limitation outlived the tool, and a
// stalled function carrying "the tools cannot speak about this" is exactly
// the note that stops the next reader looking.
//
// WHAT IS STILL WRONG, precisely, and it is a CREATION ORDER. Both compiles
// assign body 2's values from the free list r11, r10, r9, [r8 = frames], r7,
// r6, r5, r4, r3 in the order the values are created:
//
//      target  sub=r11, base=r10, sub*2=r9, sub*3=r9 (in place), base+8=r7,
//              scaled=r6, lhzx=r5, extsh=r4              -- seven registers
//      ours    sub=r11, base=r10, sub*2=r9, base+8=r7, sub*3=r6, scaled=r5,
//              lhzx=r4, extsh=r3                         -- eight
//
// The target finishes the whole INDEX chain before forming `base + 8`, so at
// the `add` the most recently allocated register is r9, sub*2 is dead, and
// the add is written in place. We form `base + 8` in the middle of the index
// chain, so r9 is no longer the top of the stack, the add takes a fresh
// register, and every value after it shifts by one.
//
// THE AND-MASK INDEX LEVER IS WHAT TOOK IT FROM 46 TO 48, and it is the only
// thing in fifty-odd measured shapes that has moved this function at all:
//
//      result = g_info[sub & 0x3FFFFFFF].size * Frames();
//
// `sub` is a u8, so any mask of 0xFF or wider changes nothing the function
// computes, and MATCHED.md's account of why it is invisible holds here --
// keeping the low bits is absorbed into the `rlwinm` that the `* 4` already
// needed, so the scaling word is byte-identical. What moves is not an operand
// order this time but the CREATION order: unmasked, MSVC matches the address
// as `base + (index << scale)` and builds the base AFTER the scaling; masked,
// the pattern misses and the base is created first, which is the target's
// order. `lis r10`, `rotlwi r9` and `addi r10,r10,9272` come into agreement
// together.
//
//      unmasked  sub=r11, sub*2=r10, base=r9, sub*3=r7, base+8=r6  46 of 53
//      masked    sub=r11, base=r10, sub*2=r9, base+8=r7, sub*3=r6  48 of 53
//
// The mask CONSTANT carries no information: 0x1FF, 0xFFFF, 0xFFFFFF,
// 0xFFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF and 0x7FFFFFFF are byte-identical, which
// is what the lever predicts. But the mask must sit IN THE SUBSCRIPT --
// naming the masked index in a local undoes it completely (368 bytes, 25 of
// 53).
//
// FLAGS ARE RULED OUT. At /O2 /Os this compiler abandons the jump table
// entirely for a compare chain (244 bytes, 4 of 61). Ten other combinations --
// /Ox, /O1-ish pieces, /Ob0, /Ob1, /Oy-, /Ot, /Og, /Oi, /GF, /Gs, /fp:precise
// -- are all byte-identical to /O2 here except /Ob0, which is worse.
//
// SHAPES MEASURED AND RULED OUT, about fifty in all. Every one is applied to
// the case-24/31 body ALONE, because body 1 is already byte-exact and
// anything that reaches the residue has to be local to body 2.
//
//   Byte-identical to the plain unmasked baseline at 46 of 53: a local for
//   the index (u8 and int), a pointer into the array, the count hoisted into
//   a local, `this->` spelled out, an explicit widening cast, a member helper
//   doing the whole lookup, a free helper doing only the lookup, the frames
//   helper as a free function, the whole function as a free function taking
//   Item*, a compound `*=`, both multiply operand orders, `default:` first,
//   last, absent, the -1 assigned in a default block, and explicit byte
//   arithmetic with the +8 written before or after the scaled index. Also
//   MATCHED.md's named const-qualified view, `const Info* t = g_info;` then
//   `t[sub].size` -- the lever that cracked VectorGrow; its stated limit is
//   that the view must name a field reached through a POINTER PARAMETER,
//   `g_info` is a global, and this is another instance of that limit. Also
//   the base as the RIGHT operand of the add, `(sub + g_info)->size`, which
//   MSVC canonicalises back. Also a TWO-LEVEL inlined free helper,
//   `InfoSize(i)` calling `InfoAt(i)`, since two nesting levels is what kept
//   a base pointer alive in sub_82164040; one level was already known not to
//   move it and two does not either.
//
//   WORSE than the baseline, and worth recording because the first of them
//   was the point of a whole batch: naming BOTH chains as locals -- the
//   declaration-order lever from sub_8216C240, worth twelve words there -- is
//   40 of 53 in EITHER order, six words worse than naming neither. A `u8`
//   index local is 24 of 53 at 368 bytes; a named entry pointer or a named
//   `short sz` is 5 of 54 at 396 bytes.
//
//   Byte-identical to the MASKED baseline at 48 of 53, so the mask is doing
//   all the work and nothing composes with it: the masked index cast to
//   `(u32)` or `(int)`, `<< 0`, `^ 0`, `+ 0`, masked twice, the base named as
//   a view first, `(&g_info[i])->size`, `(*(g_info + i)).size`, `Frames()`
//   written as the left operand, `Frames()` named in a local first, explicit
//   byte arithmetic with the mask in either association order, the index
//   chain spelled out as `(i + i*2) * 4`, and a two-level masked helper.
//
// Body 1 creates the index first in the TARGET too (k*2 -> r11, lis -> r8),
// so the two bodies genuinely differ in creation order, and the only
// source-visible difference between them is that body 2's index needs a `lbz`
// and body 1's is already live in r10.
//
// THREE OF THE MEASUREMENTS ABOVE ARE EVIDENCE, not just failed attempts:
// they pin the source shape rather than leaving it open.
//
//   * moving the case-25 group ahead of the case-24/31 group costs 12 more
//     words, so MSVC lays case bodies out in SOURCE ORDER and this order is
//     the target's;
//   * splitting 24 and 31 into two bodies costs the same 12, so they are one
//     group in the source and not two that the compiler merged;
//   * `a * b` and `b * a` compile to byte-identical code, so unlike `add`
//     and `subf`, MULLW OPERAND ORDER IS NOT READABLE -- do not spend edits
//     there. Checked on all three multiplies at once and on the middle one
//     alone.
//
//      mflr r12 ; stw r12,-8(r1) ; std r31,-16(r1) ; stwu r1,-96(r1)
//      lbz     r10,12(r3)      kind
//      li      r11,-1          result = -1
//      addi    r9,r10,-1
//      cmplwi  cr6,r9,30
//      bgt-    cr6,epilogue    outside 1..31: default
//      lis/addi/lwzx/mtctr/bctr through the table at 82726214
//
// The table sends 1..18, 20..22 and 26..30 to 82726290, 24 and 31 to
// 827262CC, 25 to 8272630C, and 19 and 23 to the epilogue -- so 19 and 23 are
// HOLES in the case list, not cases, and the three bodies are laid out in
// source order.
//
// All three bodies open with the same five instructions:
//
//      lhz     r11,14(r3) ; extsh rN,r11 ; cmpwi cr6,rN,0
//      bne-    cr6,+8     ; li rN,1
//
// which is one inlined `m_frames ? m_frames : 1` on a SIGNED short. Case 25
// keeps it in the callee-saved r31 because a call intervenes, which is also
// why the prologue saves r31 at all.
//
// The table index is built as ((k + k*2) * 4) -- a 12-byte stride -- and the
// field offset 8 stays a SEPARATE `addi` because `lhzx` has no displacement.
struct Info
{
    /* 0x00 */ char  unk0000[8];
    /* 0x08 */ short size;
    /* 0x0A */ short unk000A;
};
ASSERT_OFFSET(Info, size, 8);
ASSERT_SIZE(Info, 12);

extern Info g_info[];

struct Child;
int MeasureChild(Child* c);

struct Item
{
    /* 0x00 */ char   unk0000[4];
    /* 0x04 */ Child* child;
    /* 0x08 */ char   unk0008[4];
    /* 0x0C */ u8     kind;
    /* 0x0D */ u8     sub;
    /* 0x0E */ short  frames;

    int Frames() const;
    int Weight();
};
ASSERT_OFFSET(Item, child,  0x04);
ASSERT_OFFSET(Item, kind,   0x0C);
ASSERT_OFFSET(Item, sub,    0x0D);
ASSERT_OFFSET(Item, frames, 0x0E);

inline int Item::Frames() const
{
    short n = frames;
    return n != 0 ? n : 1;
}

int Item::Weight()
{
    int result = -1;

    switch (kind)
    {
    case 1:  case 2:  case 3:  case 4:  case 5:  case 6:
    case 7:  case 8:  case 9:  case 10: case 11: case 12:
    case 13: case 14: case 15: case 16: case 17: case 18:
    case 20: case 21: case 22:
    case 26: case 27: case 28: case 29: case 30:
        result = g_info[kind].size * Frames();
        break;

    case 24:
    case 31:
        /* The mask is inert -- `sub` is a u8 -- and is absorbed into the
           `rlwinm` the `* 4` already needed. It is here because it defeats
           MSVC's `base + (index << scale)` addressing-mode match, which is
           what creates the g_info address before the index scaling: two
           words, 46 of 53 to 48 of 53. See the header. */
        result = g_info[sub & 0x3FFFFFFF].size * Frames();
        break;

    case 25:
        result = MeasureChild(child) * Frames();
        break;
    }

    return result;
}
