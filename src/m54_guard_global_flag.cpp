// sub_821F7AF8 -- skip the work when a global byte flag is set. 32 bytes,
// 3 callers.
//
//      lis     r11,-32101
//      addi    r10,r11,-19728      -> 829AB2F0, the same global as m46
//      lbz     r9,991(r10)
//      cmplwi  cr6,r9,0
//      bnelr   cr6
//      addi    r3,r3,68
//      b       0x821F7738
//      blr                          <- unreachable, and counted in the size
//
// The address is materialised and the field offset stays in the load, which
// per src/global_field.cpp is a member of a global OBJECT. It is the same
// object src/m46_global_chain_call.cpp reads at +1084, reached from the same
// base -- second function on it, and the two agree.
//
// `lbz` plus `cmplwi`: an unsigned byte. `bnelr` is the conditional-RETURN
// idiom, so the guard is the early exit and the call is the fall-through --
// the interesting path written first.
//
// `addi r3,r3,68` with NO null test is a plain address-of a member, not an
// upcast; an upcast has to keep null null and costs `cmplwi`/`bne-`/`li 0`.
//
// 3 of 8 words are relocated.

#include "types.h"

struct Piece
{
    /* 0x00 */ u8 unk0000[68];
    /* 0x44 */ u8 body[4];
};

ASSERT_OFFSET(Piece, body, 68);

struct GlobalsF7
{
    /* 0x000 */ u8 unk0000[991];
    /* 0x3DF */ u8 suspended;
};

ASSERT_OFFSET(GlobalsF7, suspended, 991);

extern GlobalsF7 g_globalsF7;

void Submit(u8* body);

void SubmitPiece(Piece* p)
{
    if (g_globalsF7.suspended == 0)
        Submit(p->body);
}
