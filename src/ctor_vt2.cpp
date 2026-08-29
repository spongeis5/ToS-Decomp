#include "types.h"

// sub_8224DF58 -- store a vtable and zero a field. 24 B, 5 callers.
//   lis r11,-32256 ; li r10,0 ; addi r9,r11,17352
//   stw r10,12(r3) ; stw r9,0(r3) ; blr
// Store order 12 then 0, the same shape as sub_821A4628.
struct VT4458;
extern const VT4458 kVTable_8224DF58;
struct Obj4458 { const VT4458* vt; char unk0004[0x08]; s32 n; };
ASSERT_OFFSET(Obj4458, vt, 0x00);
ASSERT_OFFSET(Obj4458, n,  0x0C);
void Construct4458(Obj4458* o) { o->n = 0; o->vt = &kVTable_8224DF58; }
