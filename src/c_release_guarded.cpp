// sub_82603108 -- guard chain ending in a tail call. 64 B, 237 callers.
//
//   cmplwi cr6,r3,0
//   beqlr  cr6                        null -> return
//   lis    r11,-32093
//   lwz    r11,20944(r11)             g_enabled            (0x82A351D0)
//   cmpwi  cr6,r11,0
//   beq-   cr6,tail                   not enabled -> straight to the call
//   lis    r11,-32093
//   addi   r11,r11,18664              &g_special[0]        (0x82A348E8)
//   cmplw  cr6,r3,r11
//   beqlr  cr6
//   addi   r11,r11,20                 &g_special[1]        (0x82A348FC)
//   cmplw  cr6,r3,r11
//   beqlr  cr6
// tail:
//   li     r4,0
//   b      0x82602F08
//   blr                               unreachable
//
// Three facts read off the listing:
//
//  * The second compared address is `addi r11,r11,20` off the FIRST one, so it
//    is one relocated symbol at two offsets. Two unrelated globals could not
//    share a register, because each would carry its own relocation. That makes
//    it an array with a 20-byte stride, and the stride is what ASSERT_SIZE
//    records -- it is measured here, not assumed.
//  * `cmpwi` (signed) against 0 for the flag, `cmplw` (unsigned) for the two
//    pointer comparisons.
//  * `beq-` jumps AWAY past the two comparisons, so the comparisons are the
//    fall-through and `if (g_enabled)` is written with the positive path first.
//
// The trailing `blr` after the tail branch is unreachable and is MSVC's, not
// the source's; the recorded 64 bytes already include it.

#include "types.h"

struct SpecialObject
{
    char unk0000[20];
};
ASSERT_SIZE(SpecialObject, 20);

extern int           g_enabled;
extern SpecialObject g_special[2];

void ReleaseObject(SpecialObject*, int);

void ReleaseObjectChecked(SpecialObject* p)
{
    if (p == 0)
        return;

    if (g_enabled)
    {
        if (p == &g_special[0])
            return;
        if (p == &g_special[1])
            return;
    }

    ReleaseObject(p, 0);
}
