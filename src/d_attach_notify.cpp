#include "types.h"

// sub_825FD7C0 -- store a pointer, then notify a global system object if it
// owns this one. 60 B, 28 callers.
//
//   lis    r10,-32093
//   lwz    r9,8(r3)          o->owner, read BEFORE the store
//   mr     r11,r3            save `o`; r3 is about to become the global
//   stw    r4,44(r3)         o->item = item
//   lwz    r3,20976(r10)     -> 82A351F0, the global
//   subf   r8,r9,r3          g_sys - o->owner
//   cntlzw r7,r8
//   rlwinm r6,r7,27,31,31    the branchless == 0
//   cmplwi cr6,r6,0
//   beqlr  cr6
//   li     r6,0 ; li r5,0 ; mr r4,r11
//   b      0x8267b088
//
// The global is loaded AFTER the store because `o->item = item` might write
// the memory the global lives in; `o->owner` is loaded before it because it
// is a different offset off the same pointer.
//
// WHY THE COMPARISON IS MATERIALISED. The obvious `if (g_sys == o->owner)`
// gives `cmplw` + `bnelr` -- two words where the target spends five -- and no
// spelling of the guard as a branch reaches the target: plain, inverted,
// early-return, bool local, unsigned local, bool-returning inline predicate,
// bool-parameter inline helper, while/switch/goto, char* pointer difference,
// xor, and every optimisation level from /Od to /Ox were tried, and all of
// them fold the compare into the branch.
//
// What does NOT fold is comparing an UNSIGNED result with `> 0`. MSVC has to
// produce the 0/1 value to compare it, and only then tests it. This is the
// same free choice MATCHED.md records for sub_822D0BE8 -- `x > 0` against
// `x != 0` compile differently for an unsigned value -- and here it is worth
// five words and the whole function.
//
// The operand order of the `==` is load-bearing and is the last word to fall
// into place: MSVC emits `a == b` as `subf rD,rA,rB` computing b - a, so the
// target's `subf r8,r9,r3` (g_sys - o->owner) is `o->owner == g_sys`, not
// `g_sys == o->owner`. The other order costs exactly one word.

struct Sys;

struct Obj2
{
    /* 0x00 */ char  unk0000[0x08];
    /* 0x08 */ Sys*  owner;
    /* 0x0C */ char  unk000C[0x20];
    /* 0x2C */ void* item;
};
ASSERT_OFFSET(Obj2, owner, 0x08);
ASSERT_OFFSET(Obj2, item,  0x2C);

extern Sys* g_sys;                 /* 82A351F0 */

void Notify(Sys*, Obj2*, int, int);

void Attach(Obj2* o, void* item)
{
    o->item = item;

    unsigned same = (o->owner == g_sys);
    if (same > 0)
        Notify(g_sys, o, 0, 0);
}
