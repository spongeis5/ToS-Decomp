#include "types.h"

// sub_826C0FB8 -- 4 bytes, ONE instruction:
//
//      826C0FB8  b       0x826C0F50
//      826C0FBC  .long 0                 COMDAT padding to 8
//
// 826C0F50 is ChainNth (src/h_chain_nth.cpp), matched, taking
// (NthNode*, s32) and returning NthItem*.  An unconditional `b` with no
// register shuffling in front of it is a TAIL CALL whose arguments are
// already in place: a forwarding wrapper passing both parameters straight
// through.  Nothing adjusts r3 or r4, so the parameter list and its order are
// the callee's; nothing touches r1 or LR, so the call is the whole body.
//
// The source below compiles to exactly that -- one word, `b`, no prologue and
// no trailing `blr`, 4 bytes against the target's 4.
//
// IT IS NOT A MATCH, AND CANNOT BE ONE.  match.py exits 1 and says why:
//
//      1 word(s) compared: 0 identical, 0 differ, 1 differ in a relocated
//      word (expected)
//
//      NOT A MATCH -- all 1 word(s) are relocated, so nothing was actually
//      verified.
//
// That is 0 of 1 words verified, not 1 of 1.  The function's whole body is a
// branch displacement the linker supplies, so ANY source whose single
// instruction is a tail call produces the same object here -- the callee's
// identity, the parameter types and the return type are all unconstrained by
// comparison.  This is the case src/g_memset_thunk.cpp was kept as a worked
// example of, and the reason match.py refuses instead of reporting MATCH over
// an empty set of verified words.
//
// So this row belongs in src/attempts.txt and NOT in src/manifest.txt.  What
// is recorded here is the address's shape and its size, which the comparison
// does establish; the identification of the callee comes from the
// displacement, not from the compile.
//
// The neighbours put it in company: 826C0F50 ChainNth, then this, then
// 826C0FC0 (`lwz r3,28(r3)` -- n->value) and 826C0FC8
// (src/stride24.cpp, &n->items[i]).  Same NthNode, redeclared here.

struct NthItem
{
    char unk0000[24];
};
ASSERT_SIZE(NthItem, 24);

struct NthNode
{
    /* 0x00 */ char     unk0000[0x04];
    /* 0x04 */ NthNode* next;
    /* 0x08 */ char     unk0008[0x10];
    /* 0x18 */ NthItem* items;
    /* 0x1C */ s32      value;
};

ASSERT_OFFSET(NthNode, next,  0x04);
ASSERT_OFFSET(NthNode, items, 0x18);
ASSERT_OFFSET(NthNode, value, 0x1C);

NthItem* ChainNth(NthNode* n, s32 index);

NthItem* ChainNthFwd(NthNode* n, s32 index)
{
    return ChainNth(n, index);
}
