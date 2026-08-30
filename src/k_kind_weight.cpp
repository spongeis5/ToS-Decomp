#include "types.h"

// sub_827261D8 -- NOT MATCHED. 84 of 91 words; the seven that differ are the
// register allocation of ONE case body and nothing else. Kept for the shape,
// which is right, and for the two things it establishes about the tools.
//
// A 31-case switch on a kind byte that scales a table entry by a repeat
// count. 11 callers. The .pdata row records 60 bytes, which is the distance
// to the JUMP TABLE at 82726214; the body runs on past it to the epilogue at
// 82726340, 364 bytes in all -- and our object is 364 bytes with the table in
// the same place.
//
// MATCH.PY CANNOT SPEAK ABOUT THIS FUNCTION, whatever the source. The
// inventory lists 82726214 -- the jump table -- as a function start, and
// _is_real_start accepts it because the word before it is the `bctr` that
// reads it, which is exactly the "control cannot fall in" test. So
// can_extend's bound is 60 bytes for a 364-byte body and len(code) == tsize
// can never hold. A switch's jump table is a general blind spot in that
// heuristic, not a quirk of this address.
//
// WHAT IS STILL WRONG, precisely: in the case-24/31 body the target is one
// register tighter --
//
//      want  lis r10 ; rotlwi r9,r11,1 ; addi r10,r10,9272 ; add r9,r11,r9
//      got   lis r9  ; rotlwi r10,r11,1 ; addi r9,r9,0     ; add r7,r11,r10
//
// and every value after it shifts by one (r7/r6/r5/r4 against r6/r5/r4/r3).
// The target COALESCES the `k*3` onto the register holding `k*2`; we spend a
// fresh one. The identical body in the first case group allocates a fresh
// register in the TARGET too, so it is not a property of the expression.
//
// That is the /Os register-coalescing signature, and /Os is not available
// here: at /O2 /Os this compiler abandons the jump table entirely for a
// compare chain (244 bytes, 4 of 61). Ten other flag combinations -- /Ox,
// /O1-ish pieces, /Ob0, /Ob1, /Oy-, /Ot, /Og, /Oi, /GF, /Gs, /fp:precise --
// are all byte-identical to /O2 here except /Ob0, which is worse.
//
// Twenty source shapes were tried and none moved those seven words: a local
// for the index (u8 and int), a pointer into the array, the count hoisted
// into a local, `this->` spelled out, an explicit widening cast, a member
// helper doing the whole lookup, a free helper doing only the lookup, the
// frames helper as a free function, the whole function as a free function
// taking Item*, a compound `*=`, both multiply operand orders, `default:`
// first, last, absent, and assigning the -1 in a default block.
//
// THREE MORE, all applied to the case-24/31 body ALONE (body 1 is already
// byte-exact, so any change that reaches this has to be local to body 2), all
// BYTE-IDENTICAL to the baseline at 46 of 53:
//
//   * MATCHED.md's named const-qualified view, `const Info* t = g_info;` then
//     `t[sub].size` -- the lever that cracked VectorGrow. Its stated limit is
//     that the view must name a field reached through a POINTER PARAMETER;
//     `g_info` is a global, and this is another instance of that limit;
//   * the base as the RIGHT operand of the add, `(sub + g_info)->size`, on
//     the theory that MSVC evaluates a binary operator right to left and the
//     later-evaluated operand is created first. MSVC canonicalises it back;
//   * a TWO-LEVEL inlined free helper, `InfoSize(i)` calling `InfoAt(i)`,
//     since two nesting levels is what kept a base pointer alive in
//     sub_82164040. One level was already known not to move it; two does not
//     either.
//
// WHAT THE DIFFERENCE ACTUALLY IS, stated more precisely than "one register
// tighter". Both compiles assign the eight values of body 2 from the free
// list r11, r10, r9, [r8 = frames], r7, r6, r5, r4, r3 in CREATION order:
//
//      ours    sub=r11, sub*2=r10, base=r9,  sub*3=r7, base+8=r6, ... 8 regs
//      target  sub=r11, base=r10,  sub*2=r9, sub*3=r9, base+8=r7, ... 7 regs
//
// so the whole residue is that the target creates the g_info ADDRESS before
// the index scaling and we create it after; the in-place `add r9,r11,r9`
// follows from that, because sub*2 is then the most recently allocated
// register and is dead at the add. Body 1 creates the index first in the
// TARGET too (k*2 -> r11, lis -> r8), so the two bodies genuinely differ in
// creation order, and the only source-visible difference between them is
// that body 2's index needs a `lbz` and body 1's is already live in r10.
// Nothing tried so far reorders those two chains.
//
// THREE OF THOSE ARE EVIDENCE, and they pin the source shape rather than
// leaving it open:
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
        result = g_info[sub].size * Frames();
        break;

    case 25:
        result = MeasureChild(child) * Frames();
        break;
    }

    return result;
}
