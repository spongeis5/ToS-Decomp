#include "types.h"

// sub_821F61F8 -- dispatch on the pair, then notify the second object with
// the first one's flag byte at +78 as a 0/1 argument. 84 B.
// Bridge between m_pick_code (821F61B8) and 821F6250.
//
//      mr r30,r3 ; mr r31,r4 ; bl 0x8216cda0
//      lbz r11,78(r30) ; mr r3,r31 ; li r4,1
//      cmplwi cr6,r11,0 ; bne- over ; li r4,0
// over:bl 0x82168ea8
//
// THE ARGUMENT IS BRANCHY, so it is not `f->f4E != 0`: that spelling gives
// MATCHED.md's branchless pair `addic r10,r11,-1 ; subfe r4,r10,r11` in a
// 76-byte body. Nor is it a flag set above the test and overwritten inside
// it -- `int on = 1; if (f->f4E == 0) on = 0;` is branchless too, by a
// different route (`subfic`/`subfe`/`and`), and costs a third non-volatile.
//
// It is TWO CALLS THAT MSVC TAIL-MERGED. Written as an if/else with a
// different constant in each arm, the shared `mr r3,r31` and the `bl` are
// merged, `li r4,1` is hoisted above the test, and the else arm becomes the
// two-instruction `bne-` / `li r4,0` the image has.

struct Flags;
struct Target;

void Dispatch(Flags* f, Target* t);   /* sub_8216CDA0 */
void Notify(Target* t, int on);       /* sub_82168EA8 */

struct Flags
{
    /* 0x00 */ u8 unk0000[0x4E];
    /* 0x4E */ u8 f4E;
};
ASSERT_OFFSET(Flags, f4E, 0x4E);

void DispatchThenNotify(Flags* f, Target* t)
{
    Dispatch(f, t);

    if (f->f4E != 0)
        Notify(t, 1);
    else
        Notify(t, 0);
}
