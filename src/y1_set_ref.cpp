#include "types.h"

// sub_82636A58 -- replace a held reference at +232: add a reference to the
// incoming object, drop the one already stored, then store the new pointer.
// 88 B.  Bridge between b_free_items (82637590)'s run neighbour 82636A50 and
// 82636AB0.
//
//      mr r31,r3 ; mr r30,r4
//      cmplwi cr6,r4,0 ; beq- ; mr r3,r4 ; bl 0x8262ffc8
//      lwz r3,232(r31) ; cmplwi cr6,r3,0 ; beq- ; bl 0x82630078
//      stw r30,232(r31)
//
// Both guards branch FORWARD over their call and fall through to the store,
// so both are written as plain `if (p) Call(p);` statements with the store
// last. The held pointer is loaded straight into r3 -- one load feeding both
// the test and the call -- which is what spelling `o->held` at both places
// gives; the addref/release order is source order.

struct Ref;

void RefAdd(Ref* r);       /* sub_8262FFC8 */
void RefDrop(Ref* r);      /* sub_82630078 */

struct RefHolder
{
    /* 0x00 */ u8   unk0000[0xE8];
    /* 0xE8 */ Ref* held;
};
ASSERT_OFFSET(RefHolder, held, 0xE8);

void SetHeld(RefHolder* h, Ref* r)
{
    if (r != 0)
        RefAdd(r);
    if (h->held != 0)
        RefDrop(h->held);
    h->held = r;
}
