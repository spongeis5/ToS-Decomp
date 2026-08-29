// sub_821A5390 -- "the state is 2, or the byte flag is set". 40 B, 4 callers.
//
//      lwz     r11,204(r3)      +0xCC, SIGNED compare below
//      cmpwi   cr6,r11,2
//      beq-    cr6,set
//      lbz     r11,199(r3)      +0xC7
//      cmplwi  cr6,r11,0        UNSIGNED
//      li      r11,0
//      beq-    cr6,out
// set: li      r11,1
// out: clrlwi  r3,r11,24
//      blr
//
// THE TRAILING `clrlwi r3,r11,24` IS `bool`, NOT `u8` -- see MATCHED.md and
// src/m_state_1or2.cpp. u8, char and int returns all compute the 0/1 straight
// into r3 and stop one instruction shorter with no mask; only a bool return
// normalises, and the normalisation is what forces the value into r11 first
// so there is something to normalise from.
//
// Two guards sharing one exit, and both branch FORWARD to it, so the
// short-circuit form is the shape -- one `||` expression, not two `if`s.
// The second term's branch is `beq-` (jump to the zero when the byte IS
// zero) where m_state_1or2's is `bne-`, and that polarity difference is the
// whole of the difference between `x == 2` and `x != 0` as the second term.
//
// THIS IS NOT THE +0xD0 CLASS, and the batch note that it is was worth
// checking rather than believing. Its three matched neighbours --
// sub_821A5328 (src/c7_ready_flag.cpp), sub_821A5350 (src/m_state_1or2.cpp)
// and sub_821A5378 (src/eq2_208.cpp) -- all read an s32 at +0xD0, and
// c7_ready_flag's byte is at +0xC4. This one reads +0xCC and +0xC7. Neither
// pair differs by a constant (4 against 3), so it is not the same class seen
// through a base-subobject adjustment either; it is a different class that
// happens to be laid out next to them. Reusing StateObj here would have
// asserted a layout the bytes contradict.
//
// The s32 is compared with `cmpwi`, so it is signed -- an int, not a pointer
// (MATCHED.md: every pointer null test in this image is `cmplwi`). The byte
// is compared with `cmplwi`, so it is unsigned.
//
// Nothing is relocated; all 10 words are compared.

#include "types.h"

struct PhaseObj
{
    /* 0x000 */ char unk0000[0xC7];
    /* 0x0C7 */ u8   flag;
    /* 0x0C8 */ char unk00C8[0x04];
    /* 0x0CC */ s32  phase;

    bool IsActive() const;
};

ASSERT_OFFSET(PhaseObj, flag,  0xC7);
ASSERT_OFFSET(PhaseObj, phase, 0xCC);

bool PhaseObj::IsActive() const
{
    return phase == 2 || flag != 0;
}
