#include "types.h"

// sub_821A4FA0 -- forward to a call on a global. 16 B, 17 callers.
//   lis r11,-32102 ; mr r4,r3 ; addi r3,r11,6492 ; b 0x82603668
// 2 of 4 words are relocated.
struct GObj;
extern GObj g_obj_821A4FA0;
void Visit(GObj*, void*);
void VisitGlobal(void* p) { Visit(&g_obj_821A4FA0, p); }
