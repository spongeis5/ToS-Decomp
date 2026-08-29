#include "types.h"

// sub_828133B8 -- the same shape as sub_8288A788 with different vtables.
// 28 B, 6 callers.
//   lis r11,-32247 ; lis r10,-32247 ; addi r11,r11,-20120
//   addi r10,r10,-20048 ; stw r11,0(r3) ; stw r10,16(r3) ; b 0x8271F230
struct VTc; struct VTd;
extern const VTc kVT_828133B8_a;
extern const VTd kVT_828133B8_b;
void Chain2(void*);

struct Multi2
{
    const VTc* vt0;
    char unk0004[0x0C];
    const VTd* vt16;
    void Init();
};
ASSERT_OFFSET(Multi2, vt0,  0x00);
ASSERT_OFFSET(Multi2, vt16, 0x10);

void Multi2::Init()
{
    vt0  = &kVT_828133B8_a;
    vt16 = &kVT_828133B8_b;
    Chain2(this);
}
