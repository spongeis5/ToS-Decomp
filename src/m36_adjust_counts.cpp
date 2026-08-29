// sub_825DB4C0 -- add a delta to a set of indexed counters, then clamp the
// same set at zero. 120 bytes, 4 callers.
//
//      lwz     r11,72(r3) ; cmplwi cr6,r11,0 ; beq- cr6,<tail>
//      cmpwi   cr6,r5,0   ; ble-   cr6,<tail>
//      addi    r9,r4,-4 ; mtctr r5
//  L1: lwzu r11,4(r9) ; lwz r10,72(r3) ; rlwinm r11,r11,2,0,29
//      lwzx r8,r11,r10 ; add r8,r8,r6 ; stwx r8,r11,r10 ; bdnz+
//      cmpwi   cr6,r5,0   ; ble-   cr6,<tail>
//      mr      r9,r4 ; mtctr r5
//  L2: lwz r11,0(r9) ; lwz r10,72(r3) ; rlwinm r11,r11,2,0,29
//      lwzx r8,r11,r10 ; cmpwi cr6,r8,0 ; bge- <skip>
//      li r8,0 ; stwx r8,r11,r10
//  skip: addi r9,r9,4 ; bdnz+
//  tail: li r3,0 ; blr
//
// THREE guards branching FORWARD to one common `li r3,0 ; blr`, which per
// sub_821675B8 means the failure path is written LAST: the null test nests
// the whole body and the single `return 0` sits at the end. Written as
// `if (p == 0) return 0;` the shared tail gets planted after the first test
// and the other two guards branch backwards into it.
//
// The counter base is RELOADED on every iteration of both loops with nothing
// to explain it but aliasing -- and that is exactly the explanation: the
// store `counts[k] = ...` cannot be proven distinct from the field holding
// `counts`, so no hoist is possible. Spelling the base into a local would
// remove the reload. This is the normal shape and needs no lever.
//
// `addi r9,r4,-4` plus `lwzu` in the first loop is the biased-pointer idiom;
// the second loop cannot use it because the store is conditional, so its
// increment sits at the join instead.
//
// `cmpwi` on the count and on the clamp test: both signed ints.
//
// `/O2 /Os`, and the clamp loop says which: retail materialises the zero
// INSIDE the guarded block and reuses the register the load just vacated,
// while /O2 hoists `li r8,0` to the loop top and gives the load a fresh
// register -- the coalescing signature, worth nine words here.
//
// NEAR MISS: 29 of 30 words at /O2 /Os, right length, one word left --
//
//      want  add r8,r8,r6          the loaded counter in rA
//      got   add r8,r6,r8          the delta parameter in rA
//
// and it does NOT come out of the source. Four spellings of the same
// accumulate were measured and all four give the identical instruction:
//
//      c->counts[ids[i]] += delta;
//      c->counts[ids[i]] = delta + c->counts[ids[i]];
//      c->counts[ids[i]] = c->counts[ids[i]] + delta;
//      s32 v = c->counts[ids[i]]; c->counts[ids[i]] = v + delta;
//      an inlined `static s32 AddTo(s32 v, int d) { return v + d; }`
//
// The add-operand rule says rA holds the operand whose SOURCE read comes
// later, and it points the right way here -- `delta` is loop-invariant, so
// its CSE representative sits at function entry and the load is later -- but
// no expression order, named local or helper moves it. That is the same
// negative shape recorded for `or` on sub_8216C240: readable in principle,
// not reachable for THIS operand pair, where one side is a loop-invariant
// PARAMETER rather than a local. Every other word, both loops, both guards
// and the length are exact, so the residue is one register name.
//
// Nothing is relocated: 30 of 30 words are compared.

#include "types.h"

struct CounterSet
{
    /* 0x00 */ u8   unk0000[0x48];
    /* 0x48 */ s32* counts;
};

ASSERT_OFFSET(CounterSet, counts, 0x48);

int AdjustCounts(CounterSet* c, const s32* ids, int n, int delta)
{
    if (c->counts != 0)
    {
        for (int i = 0; i < n; i++)
            c->counts[ids[i]] += delta;

        for (int j = 0; j < n; j++)
        {
            if (c->counts[ids[j]] < 0)
                c->counts[ids[j]] = 0;
        }
    }

    return 0;
}
