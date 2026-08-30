// sub_8216C240 -- set or clear one bit of a bitset stored in a linked list of
// fixed-size word blocks. 152 B, 5 callers.
// r3 = the set, r4 = the bit index, r5 = the value.
//
//      clrlwi r8,r5,24 ; ... ; cmplwi cr6,r8,0    the bool argument, and the
//                                                 SPLIT form -- per
//                                                 sub_827156B8 `clrlwi.` is
//                                                 the /Os spelling, so this
//                                                 unit is /O2
//      lwz    r6,36(r3)                           words per block, read 2nd
//      srawi  r11,r4,5                            index >> 5, NO addze,
//                                                 so a SHIFT and not a /32
//      rotlwi r8,r11,1 ; addi r5,r8,-1 ; andc r3,r6,r5
//      twllei r6,0 ; twlgei r3,-1                 MSVC's checked signed /
//      divw.  r10,r11,r6                          ONE division; CR0 is the
//                                                 `quotient == 0` test, used
//                                                 by BOTH arms
//      mullw  r4,r6,r10                           shared by both arms
//      beq-   cr6,...                             the arm is chosen AFTER
//                                                 the division
//      subf ; mtctr/bdnz walk ; lwzx ; or/andc ; stwx   in EACH arm
//
// The trap pair is the divide guard MSVC plants for `a / b`: `twllei b,0`
// catches the zero divisor, and `rotlwi a,1 ; addi -1 ; andc b,~that ;
// twlgei ,-1` fires only when a == INT_MIN and b == -1.  Neither is written.
//
// THE `if` IS AT THE TOP OF THE SOURCE and the whole operation is written
// once per arm; MSVC then hoists what is expensive.  Three things say so and
// no other shape produces any of them: `clrlwi r8,r5,24` is the FIRST
// instruction with its `cmplwi cr6` fourth, twelve instructions ahead of the
// `beq-` that consumes it -- a comparison hoisted that far is one the
// compiler met at the top of the function; the division is the RECORD form,
// whose CR0 serves as the trip-count test in both arms; and the cheap `subf`
// and the ctr walk are duplicated while the division and the multiply are
// not.  Writing the common work above the `if` instead gives 156 bytes and
// 1 of 38 words at eight different loop spellings, with `divw` plus a
// separate `cmpwi cr6`.
//
// Only the MASK is hoisted by hand, and `w` is declared BEFORE it.  That
// ordering is worth twelve words on its own: MSVC's scheduler interleaves
// two independent chains in source order, and leading with the mask puts the
// whole bit chain ahead of the division chain -- 25 of 38 either way round
// for the mask against `w`, and 37 of 38 with `w` first.  Leaving the mask
// inside the arms duplicates it, +12 bytes.
//
// MATCHED, 38 of 38 words at /O2.  Nothing is relocated, so all 38 are
// compared.  The last word to fall was 8216C2A0,
//
//      want  or r8,r7,r9      the MASK in rS, the loaded word in rB
//      got   or r8,r9,r7      the loaded word in rS
//
// and it came out of the INDEX, not the operator.  `p->words[r & 0x3FFFFFFF]`
// in both arms is 38 of 38.
//
// SOURCE OPERAND ORDER OF `or` IS STILL NOT READABLE, and that earlier
// measurement stands -- it was the conclusion drawn from it that was wrong.
// Every spelling of the OR itself gives the identical instruction: `|=`,
// `w = w | bit`, `w = bit | w`, the word in a local either way round, the
// element's address in a local either way round, both arms through locals,
// the mask through a second local, `s32` against `u32` in all three
// combinations, two helpers with the operands swapped, a helper taking the
// element's address, a const view of the block either way round, a longer
// chain for the loaded word, and `u32* wp = p->words;` either way round.  All
// 72 flag combinations agree as well (44 give 37 of 38, 28 give 1 of 38 at
// 164 bytes).  So the operand order genuinely carries no information -- but
// it is not the only thing that sets it.
//
// WHY THE MASK REACHES IT.  MATCHED.md's AND-mask lever is recorded for
// `lwzx` and `stwx` operand order: MSVC matches `base + (index << scale)` as
// an addressing mode, a masked index misses that pattern, and the mask itself
// is absorbed into the `rlwinm` the `* 4` already needed.  Here both indexed
// accesses were ALREADY index-first and correct, so there was nothing to flip
// there -- what changed is that the missed addressing-mode pattern rebuilds
// the expression tree around the load, and the commutative `or` comes out of
// the rebuilt tree with its operands the other way round.  The scaling word
// `rlwinm r10,r8,2,0,29` is byte-identical either way, and no other word in
// the function moves.
//
// The mask changes nothing the function computes, and here that is exact
// rather than incidental: `(r & 0x3FFFFFFF) << 2` and `r << 2` are the same
// 32-bit value for EVERY r, because the two bits the mask clears are the two
// the shift discards.  So this is not the sub_826DD4A0 case where the mask
// happened to wrap to the same address on one path -- it is an identity.
//
// The `andc` in the other arm is not commutative, which is why only the one
// word was ever exposed.

#include "types.h"

struct Block
{
    /* 0x00 */ u32*   words;
    /* 0x04 */ Block* next;
};

struct ChunkedSet
{
    /* 0x00 */ char   unk0000[36];
    /* 0x24 */ s32    perBlock;
    /* 0x28 */ char   unk0028[4];
    /* 0x2C */ Block* head;
};
ASSERT_OFFSET(ChunkedSet, perBlock, 36);
ASSERT_OFFSET(ChunkedSet, head, 44);

void ChunkedSetBit(ChunkedSet* s, s32 index, bool on)
{
    s32 w = index >> 5;
    u32 bit = 1u << (index & 31);

    if (on)
    {
        s32 q = w / s->perBlock;
        s32 r = w - q * s->perBlock;
        Block* p = s->head;
        for (s32 i = q; i != 0; i--)
            p = p->next;
        p->words[r & 0x3FFFFFFF] |= bit;
    }
    else
    {
        s32 q = w / s->perBlock;
        s32 r = w - q * s->perBlock;
        Block* p = s->head;
        for (s32 i = q; i != 0; i--)
            p = p->next;
        p->words[r & 0x3FFFFFFF] &= ~bit;
    }
}
