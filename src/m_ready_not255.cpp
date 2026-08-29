#include "types.h"

// sub_82265D88 -- guarded "is it not 255". 40 B, 8 callers.
//
//      lwz     r11,12(r3)
//      cmplwi  cr6,r11,0
//      bne-    cr6,test
//      li      r3,0
//      blr
// test:lwz     r11,16(r3)
//      addi    r11,r11,-255
//      addic   r10,r11,-1
//      subfe   r3,r10,r11
//      blr
//
// `addic rD,rS,-1 ; subfe rT,rD,rS` is the branchless `!= 0`, and it is
// worth writing out because reading it as arithmetic wastes time:
//
//   rS == 0  ->  addic gives -1 with NO carry, subfe gives 0 - (-1) - 1 + 0 = 0
//   rS != 0  ->  addic carries,               subfe gives 0 + 1             = 1
//
// So the tail is `(x - 255) != 0`, which is `x != 255`.
//
// The `bne-` jumps AWAY to that tail, so the `return 0` is the fall-through
// and the guard has to be written first.
struct Ready
{
    char  unk0000[12];
    void* owner;
    s32   state;
};
ASSERT_OFFSET(Ready, owner, 0x0C);
ASSERT_OFFSET(Ready, state, 0x10);

int IsReady(const Ready* r)
{
    if (r->owner == 0)
        return 0;
    return r->state != 255;
}
