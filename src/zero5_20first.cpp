#include "types.h"

// sub_82548F10 -- zero five words, starting with the LAST. 28 B, 7 callers.
//   li r11,0 ; stw r11,20(r3) ; stw r11,4(r3) ; stw r11,8(r3)
//   stw r11,12(r3) ; stw r11,16(r3) ; blr
// Written in the target's own order: 20, then 4, 8, 12, 16.
struct Five { char unk0000[0x04]; s32 a; s32 b; s32 c; s32 d; s32 e; };
ASSERT_OFFSET(Five, a, 0x04);
ASSERT_OFFSET(Five, e, 0x14);
void ClearFive(Five* f)
{
    f->e = 0;
    f->a = 0; f->b = 0; f->c = 0; f->d = 0;
}
