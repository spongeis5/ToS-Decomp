// sub_822547C8 -- apply a requested state to an object: a per-state fast
// path that returns, otherwise report the old state, report its handler, and
// commit. 624 B, 10 callers.
//
// THE RECORDED SIZE OF 136 BYTES IS WRONG, and so is the batch's row: the
// inventory is truncated by the jump table that follows the `bctr`.
// build/switch_tables.txt has 44 bytes of table at 82254850 and .pdata gives
// the real body as 624 bytes -- 156 instructions, of which eleven are table.
//
// The table holds ABSOLUTE addresses (FINDINGS 7v's third dispatch form), so
// the arms read straight out of it: 1 -> 82254908, 5 -> 822548DC,
// 9 -> 8225487C, 10 -> 8225489C, 11 -> 822548BC, everything else -> 82254924.
// BLOCK ORDER is 9, 10, 11, 5, 1 and MSVC lays case bodies out in source
// order, so that is the order they are written in.
//
// Every arm's failure edge lands on 82254924 -- the same block the default
// reaches -- and every success edge is a `b 82254A20` to the epilogue. So
// each arm RETURNS on success and `break`s on failure, and 82254924 is the
// code after the switch rather than a default body.
//
// THE SECOND SWITCH IS THE `bdzf` CHAIN at 8225496C, which is MSVC's dense
// small-switch on CTR rather than a table:
//
//      mtctr r11 ; cmpwi cr6,r11,0        r11 = kind - 1, already <= 4
//      bdzf- 4*cr6+eq,L2                  taken only when r11 == 1
//      bdzf- 4*cr6+eq,L3                  taken only when r11 == 2
//      bdzf- 4*cr6+eq,L4                  taken only when r11 == 3
//      bne-  cr6,L5                       r11 is now 0 or 4
//
// Each `bdzf` decrements CTR and branches when it reaches zero AND cr6's EQ
// is clear, and cr6.eq is set only for r11 == 0 -- so arm N is reached after
// exactly N decrements. Decoded, the five arms load 0x958, 0x960, 0x95C,
// 0x964 and 0x968 for kind 1..5. That order is not monotonic, which is what
// rules out an array: 0x958 + 4*i would need the index sequence 0,2,1,3,4.
//
// The out-of-range edge of that switch, the `state == kind` edge and the
// `handler == 0` edge all land on 822549CC, so the default arm produces a
// null handler and the shared `if` after it does the rest.
//
// 829AB2F0 is formed with `lis`+`addi` and the store then uses a
// displacement of 1453, which per MATCHED.md is a FIELD INSIDE a global
// object, not a folded constant -- a two-instruction `lis`/`stb` would have
// reached the same byte. 82002DA4 is 0.0f.
//
// The `cmpwi` on the state parameter and on `kind` against `cmplwi` on the
// two byte fields is the ordinary signedness split: `state` and `kind` are
// s32, `pendingClear` is u8.

#include "types.h"

struct StateHost
{
    /* 0x000 */ u8    unk0000[0x8C0];
    /* 0x8C0 */ s32   guard;
    /* 0x8C4 */ u8    unk08C4[0x14];
    /* 0x8D8 */ s32   kind;
    /* 0x8DC */ s32   prev;
    /* 0x8E0 */ u8    pendingClear;
    /* 0x8E1 */ u8    unk08E1[0x03];
    /* 0x8E4 */ f32   timer;
    /* 0x8E8 */ u8    unk08E8[0x70];
    /* 0x958 */ void* h1;
    /* 0x95C */ void* h2;
    /* 0x960 */ void* h3;
    /* 0x964 */ void* h4;
    /* 0x968 */ void* h5;
};
ASSERT_OFFSET(StateHost, guard,        0x8C0);
ASSERT_OFFSET(StateHost, kind,         0x8D8);
ASSERT_OFFSET(StateHost, prev,         0x8DC);
ASSERT_OFFSET(StateHost, pendingClear, 0x8E0);
ASSERT_OFFSET(StateHost, timer,        0x8E4);
ASSERT_OFFSET(StateHost, h1,           0x958);
ASSERT_OFFSET(StateHost, h5,           0x968);

struct Registry
{
    /* 0x000 */ u8 unk0000[1453];
    /* 0x5AD */ u8 pending;
};
ASSERT_OFFSET(Registry, pending, 1453);

extern u8       g_mode;              /* 829A109A */
extern Registry g_registry;          /* 829AB2F0 */

void Apply(StateHost* o, s32 state);                                  /* 82254760 */
void ReportState(StateHost* o, s32 a, u32 id, void* p, s32 n, s32 f); /* 822E1FE8 */
void ReportHandler(void* a, s32 b, void* h, u32 id, s32 n, s32 f);    /* 8217E048 */

void ApplyState(StateHost* o, s32 state)
{
    if ((g_mode & 8) != 0 && state == 0 && o->guard == 0)
        state = 1;

    if (o->pendingClear != 0 && state == 0)
        o->pendingClear = 0;

    switch (state)
    {
    case 9:
        if (o->kind == 2 || o->kind == 9)
        {
            Apply(o, o->kind);
            return;
        }
        break;

    case 10:
        if (o->kind == 3 || o->kind == 10)
        {
            Apply(o, o->kind);
            return;
        }
        break;

    case 11:
        if (o->kind == 4 || o->kind == 11)
        {
            Apply(o, o->kind);
            return;
        }
        break;

    case 5:
        g_registry.pending = 1;
        if (o->kind == 5)
        {
            Apply(o, 5);
            return;
        }
        break;

    case 1:
        if (o->kind == 1)
        {
            Apply(o, 1);
            return;
        }
        break;
    }

    if (o->kind != 0)
    {
        u8 was = 0;
        ReportState(o, 0, 0xFEF69755u, &was, 219, 1);
    }

    if (state != o->kind)
    {
        void* h;
        switch (o->kind)
        {
        case 1:  h = o->h1; break;
        case 2:  h = o->h3; break;
        case 3:  h = o->h2; break;
        case 4:  h = o->h4; break;
        case 5:  h = o->h5; break;
        default: h = 0;     break;
        }

        if (h != 0)
            ReportHandler(0, 0, h, 0x5D2F4217u, 0, 1);
    }

    s32 prev = o->prev;
    o->kind = state;
    if (prev != state)
    {
        o->timer = 0.0f;
    }
    else
    {
        Apply(o, state);
        u8 now = (u8)o->kind;
        ReportState(0, 0, 0xFEF69755u, &now, 219, 1);
    }
}
