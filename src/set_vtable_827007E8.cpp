#include "types.h"

// sub_827007E8 -- store a fixed address into the first field. 16 B, 32 callers.
//   lis r11,-32248 ; addi r11,r11,-11828 ; stw r11,0(r3) ; blr
//
// The free-function form produces `addi r10,r11` and stores r10; the target
// reuses r11. Its two nearest neighbours, 826FE5B8 and 826FE5C8, are the same
// idiom and DO use r10, which is why a per-function flag was rejected as an
// explanation (MATCHED.md). The member form is the other lever.
struct VTable827;
extern const VTable827 kVTable_827007E8;

struct Object827
{
    const VTable827* vt;
    void Init();
};
ASSERT_OFFSET(Object827, vt, 0x00);

void Object827::Init() { vt = &kVTable_827007E8; }
