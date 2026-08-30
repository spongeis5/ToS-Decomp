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
// MATCHED, 30 of 30 words at /O2 /Os. Nothing is relocated, so 30 of 30 are
// compared.
//
// The last word to fall was
//
//      want  add r8,r8,r6          the loaded counter in rA
//      got   add r8,r6,r8          the delta parameter in rA
//
// and the previous attempt recorded it as unreachable, on the reading that
// `delta` is a loop-invariant PARAMETER whose CSE representative is pinned at
// function entry. The measurement was right and the conclusion was not: the
// expression axis really is dead -- all five of
//
//      c->counts[ids[i]] += delta;
//      c->counts[ids[i]] = delta + c->counts[ids[i]];
//      c->counts[ids[i]] = c->counts[ids[i]] + delta;
//      s32 v = c->counts[ids[i]]; c->counts[ids[i]] = v + delta;
//      an inlined `static s32 AddTo(s32 v, int d) { return v + d; }`
//
// still give `add r8,r6,r8`, and so do `s32 d = delta;` declared inside the
// loop in either order against `v`, an unsigned round trip, a helper with the
// parameters swapped, an index local, and a `while` rewrite. What moves it is
// not the ADD at all but the operand's PROVENANCE, and three separate levers
// each reach it, all giving byte-identical code:
//
//      s32* p = &c->counts[ids[i]]; *p += delta;    the address-of pin
//      s32 d = delta;  at the top of the function   a named parameter copy
//      const CounterSet* cc = c;  for the load      the const-view lever
//
// So SEVEN spellings now compile to these exact 120 bytes (the three above,
// plus `*p = *p + delta`, `*p = delta + *p`, the pin in one loop or in both).
// The bytes therefore do NOT decide which was written; the pin is taken here
// because it is the one that reads as ordinary C, and the alternatives are
// recorded so the choice is not mistaken for a measurement.
//
// The base reload survives the pin -- `c->counts` is still read inside the
// loop -- which is what keeps the `lwz r10,72(r3)` in both loop bodies.
//
// /O2 alone is 22 of 30: the clamp loop's `li r8,0` hoists out of the guarded
// block and the load gets a fresh register, nine words, which is the
// coalescing signature that says /Os.

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
        {
            s32* p = &c->counts[ids[i]];
            *p += delta;
        }

        for (int j = 0; j < n; j++)
        {
            s32* p = &c->counts[ids[j]];
            if (*p < 0)
                *p = 0;
        }
    }

    return 0;
}
