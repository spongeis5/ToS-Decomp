#include "types.h"

// sub_8225B450 -- "is the state 2, 3 or 4". 44 B, 8 callers.
//
//      lwz     r11,8(r3)
//      lwz     r11,2264(r11)
//      cmpwi   cr6,r11,2 ; beq- set
//      cmpwi   cr6,r11,3 ; beq- set
//      cmpwi   cr6,r11,4
//      li      r3,0
//      bnelr   cr6
// set: li      r3,1
//      blr
//
// Three compares against consecutive constants and no range check, so the
// source really is a chain of `==` rather than `x - 2 <= 2`. No `clrlwi` on
// the way out, so the return is int and not a byte -- unlike sub_821A5350,
// which is the same shape one type narrower and is still unmatched because
// of it.
//
// 2264 is 0x8D8, the same offset sub_82255408 reads. One state field, two
// readers.
struct Session
{
    char unk0000[0x8D8];
    s32  phase;
};
ASSERT_OFFSET(Session, phase, 0x8D8);

struct Actor
{
    char     unk0000[8];
    Session* session;
};
ASSERT_OFFSET(Actor, session, 0x08);

int IsRunning(const Actor* a)
{
    s32 p = a->session->phase;
    return (p == 2 || p == 3 || p == 4);
}
