// sub_821A5160 -- copy a 12-byte vector into a field at +116. 28 bytes,
// 3 callers.
//
//      lwz r11,0(r4) ; stw r11,116(r3)
//      lwz r10,4(r4) ; stw r10,120(r3)
//      lwz r9,8(r4)  ; stw r9,124(r3)
//      blr
//
// Integer loads and stores for what the offsets say is a vector: MSVC copies
// a small POD by WORDS and never looks at the member types, so this is one
// struct assignment and not three float assignments -- those would be
// lfs/stfs, as the last three stores of src/m29_set_frame_axes.cpp are.
//
// Nothing is relocated: 7 of 7 words are compared.

#include "types.h"

struct Vec3
{
    f32 x;
    f32 y;
    f32 z;
};

ASSERT_SIZE(Vec3, 12);

struct VecHolder
{
    /* 0x00 */ u8   unk0000[0x74];
    /* 0x74 */ Vec3 value;
};

ASSERT_OFFSET(VecHolder, value, 116);

void SetValue(VecHolder* h, const Vec3* v)
{
    h->value = *v;
}
