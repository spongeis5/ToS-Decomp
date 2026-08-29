#include "types.h"

// sub_827C4FB0 -- offset a pointer, or null. 24 B, 5 callers.
//   lwz r11,668(r3) ; cmplwi cr6,r11,0 ; addi r3,r11,28
//   bnelr cr6 ; li r3,0 ; blr
// The addi is computed BEFORE the test, so the non-null path falls out of
// the conditional return with the value already in place.
struct Sub28;
struct Own668 { char unk0000[0x29C]; Sub28* sub; };
ASSERT_OFFSET(Own668, sub, 0x29C);
char* SubPlus28(Own668* o)
{
    char* p = (char*)o->sub;
    if (p)
        return p + 28;
    return 0;
}
