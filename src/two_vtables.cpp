#include "types.h"

// sub_8288A788 -- store two vtable pointers, then tail-call. 28 B, 6 callers.
//   lis r11,-32247 ; lis r10,-32247 ; addi r11,r11,884 ; addi r10,r10,800
//   stw r11,0(r3) ; stw r10,16(r3) ; b 0x8271F230
// Both addresses are materialised before either store. The free-function
// form put the addi results in r9/r8; the target reuses r11/r10.
struct VTa; struct VTb;
extern const VTa kVT_8288A788_a;
extern const VTb kVT_8288A788_b;
void Chain(void*);

struct Multi
{
    const VTa* vt0;
    char unk0004[0x0C];
    const VTb* vt16;
    void Init();
};
ASSERT_OFFSET(Multi, vt0,  0x00);
ASSERT_OFFSET(Multi, vt16, 0x10);

void Multi::Init()
{
    vt0  = &kVT_8288A788_a;
    vt16 = &kVT_8288A788_b;
    Chain(this);
}
