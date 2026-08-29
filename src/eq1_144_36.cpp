#include "types.h"

// sub_821A93C8 -- the same idiom again. 24 B, 6 callers.
//   lwz r11,144(r3) ; lwz r11,36(r11) ; addi r10,r11,-1
//   cntlzw r9,r10 ; rlwinm r3,r9,27,31,31 ; blr
struct Sub36  { char unk0000[0x24]; s32 kind; };
struct Own144 { char unk0000[0x90]; Sub36* sub; };
ASSERT_OFFSET(Sub36,  kind, 0x24);
ASSERT_OFFSET(Own144, sub,  0x90);
int IsKind1(Own144* p) { return p->sub->kind == 1; }
