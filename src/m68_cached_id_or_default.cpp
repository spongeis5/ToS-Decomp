// sub_82251218 -- return a cached id, or one of two fallbacks. 56 bytes,
// 3 callers.
//
//      lis    r11,-32101 ; lwz r3,8596(r11)      -> 829B2194
//      cmplwi cr6,r3,0 ; bnelr cr6
//      lis    r11,-32101 ; addi r11,r11,-19728   -> 829AB2F0
//      lbz    r10,1220(r11) ; cmplwi cr6,r10,0 ; beq- cr6,<other>
//      lis    r3,19790 ; ori r3,r3,21843         = 4D4E5553
//      blr
//  other:
//      lwz    r3,908(r11) ; blr
//
// TWO globals with the same relocated high half and neither shares it. The
// first folds its low half straight into the `lwz`, which only happens at
// offset 0 -- so it is a variable in its own right. The second gets an `addi`
// and then two field offsets, which is a global OBJECT, and it is the same
// object src/m46 reads at +1084 and src/m54 at +991. Third function on it.
//
// `lis`+`ori` is how MSVC builds a 32-bit literal that does not fit in 16
// bits: 4D4E5553.
//
// `bnelr` loads the cached value straight into r3 and returns it there, which
// is the early-return spelling -- a single return through an accumulator
// would have materialised the fallbacks first.
//
// 4 of 14 words are relocated.

#include "types.h"

struct IdGlobals
{
    /* 0x000 */ u8  unk0000[908];
    /* 0x38C */ u32 defaultId;
    /* 0x390 */ u8  unk0390[1220 - 912];
    /* 0x4C4 */ u8  useLiteral;
};

ASSERT_OFFSET(IdGlobals, defaultId, 908);
ASSERT_OFFSET(IdGlobals, useLiteral, 1220);

extern IdGlobals g_idGlobals;
extern u32       g_cachedId;

u32 CurrentId()
{
    u32 id = g_cachedId;
    if (id != 0)
        return id;

    if (g_idGlobals.useLiteral)
        return 0x4D4E5553;

    return g_idGlobals.defaultId;
}
