// sub_821F6B70 -- set a holder's state byte, and if it changed to non-zero
// reset two floats, raise a flag and clear the holder's row in a global
// table; then, if the state is back to zero, tail-call sub_821F6B30.
// 208 B, 4 callers.  BRIDGE: between Acc_821F6B68 and ReleaseSlot
// (sub_821F6C40), both matched.
//
// The class is the one src/g7_release_slot.cpp calls `Holder`: +131 is the
// busy byte and +132 the id, at the same offsets and with the same
// signedness -- `cmpwi` on the id here as there, so it is an s32.
//
// The global table is the same one, at 829A79B4, and the stride is 36:
// `rlwinm 3` + `add` + `rlwinm 2` is (id + id*8) * 4.  Rows +30 (u16) and
// +32 (u8) are cleared.  Both the id and the table pointer are RELOADED for
// the second row store, because the first store is through a pointer the
// compiler cannot prove disjoint from either -- so both reads are spelled
// out rather than held in locals.
//
// 82002DA4 = 0.0f, 82002D40 = 1.0f.  The 0.0f is loaded ONCE, at the join
// after the first guard, and serves both the store to +148 and the `fcmpu`
// at the very end -- so it is the same literal in both places.

#include "types.h"

struct Leaf
{
    /* 0x00 */ char unk0000[0x2C];
    /* 0x2C */ f32  f2C;
};
ASSERT_OFFSET(Leaf, f2C, 0x2C);

struct Owner
{
    /* 0x00 */ char  unk0000[0x48];
    /* 0x48 */ Leaf* leaf;
};
ASSERT_OFFSET(Owner, leaf, 0x48);

struct Row
{
    /* 0x00 */ char unk0000[0x1E];
    /* 0x1E */ u16  f1E;
    /* 0x20 */ u8   f20;
    /* 0x21 */ char unk0021[0x03];
};
ASSERT_OFFSET(Row, f1E, 0x1E);
ASSERT_OFFSET(Row, f20, 0x20);
ASSERT_SIZE(Row, 36);

extern Row* g_rows;                    /* 829A79B4 */

struct Holder
{
    /* 0x000 */ Owner* owner;
    /* 0x004 */ char   unk0004[0x7C];
    /* 0x080 */ u8     active;
    /* 0x081 */ u8     state;
    /* 0x082 */ char   unk0082[0x01];
    /* 0x083 */ u8     busy;
    /* 0x084 */ s32    id;
    /* 0x088 */ char   unk0088[0x0C];
    /* 0x094 */ f32    f94;
    /* 0x098 */ char   unk0098[0x08];
    /* 0x0A0 */ f32    fA0;
};
ASSERT_OFFSET(Holder, active, 0x80);
ASSERT_OFFSET(Holder, state,  0x81);
ASSERT_OFFSET(Holder, busy,   0x83);
ASSERT_OFFSET(Holder, id,     0x84);
ASSERT_OFFSET(Holder, f94,    0x94);
ASSERT_OFFSET(Holder, fA0,    0xA0);

void Mark(Holder* h);                  /* sub_821F6B30 */

void SetState(Holder* h, u8 v)
{
    if (v != 0)
    {
        if (h->busy == 0)
            return;
        if (h->id == 0)
            return;
    }

    if (v != h->state)
    {
        h->state = v;
        if (v != 0)
        {
            h->f94    = 0.0f;
            h->active = 1;
            h->fA0    = 1.0f;
            g_rows[h->id].f20 = 0;
            g_rows[h->id].f1E = 0;
        }
    }

    if (h->state != 0)
        return;
    if (h->active == 0)
        return;
    if (h->owner->leaf->f2C != 0.0f)
        return;

    Mark(h);
}
