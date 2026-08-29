// sub_822B7F60 -- enter a state: run the per-state entry work, then record
// the new state and reset a timer. 352 B, 14 callers.
//
// THE RECORDED SIZE OF 64 BYTES IS WRONG and so is the batch's row: the
// inventory is truncated by the jump table that follows the `bctr`.
// build/switch_tables.txt has 40 bytes of table at 822B7FA0 and .pdata gives
// the real body as 352 bytes -- 88 instructions, of which ten are table.
//
//      addi   r11,r4,-3 ; cmplwi cr6,r11,9 ; bgt- tail
//      lis/addi r12 = 822B7FA0 ; rlwinm r0,r11,2,0,29
//      lwzx   r0,r12,r0 ; mtctr r0 ; bctr
//
// The table holds ABSOLUTE addresses -- the third dispatch form, the one
// FINDINGS 7v records as the most common -- so the arms are read straight
// out of it:
//
//      3 -> 822B8020   4,5,7,8 -> tail   6 -> 822B8014   9 -> 822B7FC8
//     10 -> 822B8014  11 -> 822B8030    12 -> 822B8078
//
// and the BLOCK ORDER is 9, {6,10}, 3, 11, 12. MSVC lays case bodies out in
// source order and does not invent groups (MATCHED.md, measured on
// sub_827261D8), so that is the order they are written in and 6 and 10 were
// written as one group.
//
// 8200332C is 0x30000000 = 2^-31 exactly, so the random word is normalised
// into 0..1 -- and the `clrldi` before the std/lfd/fcfid/frsp says the value
// is UNSIGNED. The two calls nest: sub_8218C5C0's result is left in r3 and
// consumed by sub_82156F38 with no move.
//
// In the case-11 chain each pointer is loaded once and the last lands
// directly in r3 with no `mr`, which per MATCHED.md's un-naming lever is the
// short-circuit `&&` form rather than three separate `if`s.
//
// The tail is shared by the default and by every `break`; 82002DA4 is 0.0f.

#include "types.h"

struct Node;
struct Target;

struct Child
{
    /* 0x00 */ u8    unk0000[0x0C];
    /* 0x0C */ void* handler;
};
ASSERT_OFFSET(Child, handler, 0x0C);

struct Chain
{
    /* 0x00 */ u8     unk0000[0x4C];
    /* 0x4C */ Child* child;
};
ASSERT_OFFSET(Chain, child, 0x4C);

struct Owner
{
    /* 0x00 */ u8 unk0000[0x8C];
    /* 0x8C */ u8 kind;
};
ASSERT_OFFSET(Owner, kind, 0x8C);

struct StateObj
{
    /* 0x00 */ u8      unk0000[0x26];
    /* 0x26 */ u16     flags;
    /* 0x28 */ u8      unk0028[0x10];
    /* 0x38 */ Chain*  chain;
    /* 0x3C */ u8      unk003C[0x08];
    /* 0x44 */ Owner*  owner;
    /* 0x48 */ u8      unk0048[0x10];
    /* 0x58 */ Target* target;
    /* 0x5C */ u8      unk005C[0x04];
    /* 0x60 */ s32     state;
    /* 0x64 */ u8      unk0064[0x04];
    /* 0x68 */ f32     timer;
    /* 0x6C */ u8      kind;
    /* 0x6D */ u8      unk006D[0x0B];
    /* 0x78 */ f32     roll;
};
ASSERT_OFFSET(StateObj, flags,  0x26);
ASSERT_OFFSET(StateObj, chain,  0x38);
ASSERT_OFFSET(StateObj, owner,  0x44);
ASSERT_OFFSET(StateObj, target, 0x58);
ASSERT_OFFSET(StateObj, state,  0x60);
ASSERT_OFFSET(StateObj, timer,  0x68);
ASSERT_OFFSET(StateObj, kind,   0x6C);
ASSERT_OFFSET(StateObj, roll,   0x78);

void SetPending(Target* t, s32 v);          /* 825FD7C0 */
void Detach(StateObj* o);                   /* 8224C888 */
u32  StreamOf(Chain* c);                    /* 8218C5C0 */
u32  NextRandom(u32 stream);                /* 82156F38 */
void Halt(Chain* c);                        /* 8218C658 */
void Release(void* handler);                /* 82181668 */

void EnterState(StateObj* o, s32 state)
{
    switch (state)
    {
    case 9:
        SetPending(o->target, 31);
        o->roll = (f32)NextRandom(StreamOf(o->chain)) * 4.6566128730773926e-010f;
        o->kind = o->owner->kind;
        break;

    case 6:
    case 10:
        Detach(o);
        break;

    case 3:
        SetPending(o->target, 0);
        break;

    case 11:
        Detach(o);
        Halt(o->chain);
        SetPending(o->target, 31);
        if (o->chain != 0 && o->chain->child != 0 && o->chain->child->handler != 0)
            Release(o->chain->child->handler);
        break;

    case 12:
        Halt(o->chain);
        SetPending(o->target, 31);
        o->flags = (u16)(o->flags & ~1);
        break;
    }

    o->state = state;
    o->timer = 0.0f;
}
