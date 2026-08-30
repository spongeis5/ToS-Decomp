// sub_821C1D58 -- mark one of eight bits in a global byte and hand off to
// that slot's own routine. 240 bytes, 1 caller.
//
// The dispatch is NOT a jump table. MSVC's dense small-switch form loads the
// value into CTR and walks a chain of `bdzf`:
//
//      cmplwi cr6,r3,7 ; bgtlr cr6        out of range -> return
//      mtctr  r3 ; cmpwi cr6,r3,0
//      bdzf-  4*cr6+eq,<case 1>           CTR-- ; taken when CTR hit 0 and
//      bdzf-  4*cr6+eq,<case 2>           the value was not 0
//      ... five more ...
//      bne-   cr6,<case 7>
//      <case 0 falls through>
//
// so the n-th `bdzf` fires for value n, the trailing `bne-` takes 7, and 0
// is the fall-through -- the only value for which cr6.eq is set and CTR
// never reaches zero on the way down.
//
// Each arm is the same five instructions and they appear in the image in
// case order 0..7:
//
//      lis/li r3,1 ; lbz r9,4249(r10) ; ori r11,r9,<bit> ; stb ; b <target>
//
// with the bits running 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 and
// eight distinct tail-call targets, every one of them a `.pdata` function
// start. `b` rather than `bl` plus the unreachable `blr` at 821C1E44 is the
// tail call MSVC leaves behind for a void arm whose last statement is a
// call.
//
// The flag byte is at 829A1099, reached by `lis`/`lbz` with a displacement,
// which per src/b_fwd_global5.cpp is a global VARIABLE and not the address
// of an object. `li r3,1` is issued BEFORE the read-modify-write in every
// arm: the argument register is free and the store is the longer chain.

#include "types.h"

extern u8 g_slotMask;          /* 829A1099 */

void SlotEnable0(int on);      /* 821A8628 */
void SlotEnable1(int on);      /* 821A8660 */
void SlotEnable2(int on);      /* 821A8678 */
void SlotEnable3(int on);      /* 821A8760 */
void SlotEnable4(int on);      /* 821A87E8 */
void SlotEnable5(int on);      /* 821A8848 */
void SlotEnable6(int on);      /* 821A8860 */
void SlotEnable7(int on);      /* 821A8878 */

void EnableSlot(u32 slot)
{
    switch (slot)
    {
    case 0:
        g_slotMask |= 0x80;
        SlotEnable0(1);
        break;
    case 1:
        g_slotMask |= 0x40;
        SlotEnable1(1);
        break;
    case 2:
        g_slotMask |= 0x20;
        SlotEnable2(1);
        break;
    case 3:
        g_slotMask |= 0x10;
        SlotEnable3(1);
        break;
    case 4:
        g_slotMask |= 0x08;
        SlotEnable4(1);
        break;
    case 5:
        g_slotMask |= 0x04;
        SlotEnable5(1);
        break;
    case 6:
        g_slotMask |= 0x02;
        SlotEnable6(1);
        break;
    case 7:
        g_slotMask |= 0x01;
        SlotEnable7(1);
        break;
    }
}
