#include "types.h"

// sub_821A4FB0 -- the same with a constant third argument. 20 B, 9 callers.
//   lis r11,-32102 ; mr r4,r3 ; li r5,14 ; addi r3,r11,6492 ; b 0x82609740
// Adjacent to sub_821A4FA0 and referencing the same global.
struct GObj2;
extern GObj2 g_obj_821A4FB0;
void VisitN(GObj2*, void*, int);
void VisitGlobal14(void* p) { VisitN(&g_obj_821A4FB0, p, 14); }
