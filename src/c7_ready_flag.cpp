// sub_821A5328 -- "no state set, and the byte flag is on". 40 bytes,
// 5 callers.
//
//      lwz     r11,208(r3)         +0xD0
//      cmpwi   cr6,r11,0           SIGNED
//      bne-    cr6,zero
//      lbz     r11,196(r3)         +0xC4
//      cmplwi  cr6,r11,0           UNSIGNED
//      li      r11,1
//      bne-    cr6,out
// zero:li      r11,0
// out: clrlwi  r3,r11,24
//      blr
//
// The trailing `clrlwi r3,r11,24` is the `bool` signature -- NOT `u8`. See
// MATCHED.md: u8, char and int returns all compute the 0/1 straight into r3
// and stop one instruction shorter with no mask; only `bool` normalises, and
// the normalisation is what forces the value into r11 first.
//
// This is the immediate neighbour of sub_821A5350 (src/m_state_1or2.cpp,
// matched) and sub_821A5378 (src/eq2_208.cpp, matched): 821A5328 + 40 =
// 821A5350 + 36 = 821A5374, next start 821A5378. All three take the same
// pointer in r3 and read the same s32 at +0xD0 with a SIGNED `cmpwi`, so
// they are methods of one class and the field is an `int`, not a pointer.
// Written as a member function for the same reason m_state_1or2 is.
//
// The value tested at +0xC4 is a byte compared with `cmplwi`, so it is
// unsigned.
//
// Nothing is relocated; all 10 words are compared.

#include "types.h"

struct StateObj
{
    /* 0x000 */ char unk0000[0xC4];
    /* 0x0C4 */ u8   ready;
    /* 0x0C5 */ char unk00C5[0x0B];
    /* 0x0D0 */ s32  state;

    bool IsIdleAndReady() const;
};

ASSERT_OFFSET(StateObj, ready, 0xC4);
ASSERT_OFFSET(StateObj, state, 0xD0);

bool StateObj::IsIdleAndReady() const
{
    return state == 0 && ready != 0;
}
