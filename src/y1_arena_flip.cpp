#include "types.h"

// sub_82606F68 -- the second half of ArenaAlloc on its own: zero every
// registered back-pointer, then flip the double-buffered arena if anything
// has been allocated out of the current buffer. 108 B.
// Bridge between ArenaAlloc (82606EC8) and its twin (82606FD8), so the
// layout is the one f_arena_alloc.cpp already established -- the same
// g_arena at 82A35288 and the same g_fixups at 82A352A0.
//
//      addi r9,r11,21152 ; lwz r10,21152(r11) ; lwz r11,4(r9)
//      rlwinm r10,r10,2,0,29 ; add r10,r10,r11 ; cmplw ; beq-
//   L: lwz r9,0(r11) ; addi r11,r11,4 ; cmplw ; stw r8,0(r9) ; bne+ L
//      addi r11,r11,21128 ; lwz r7,12(r11) ; cmpwi cr6,r7,0 ; blelr cr6
//      lwz r10,0(r11) ; lwz r9,4(r11)
//      stw r8,16(r11) ; stw r7,20(r11) ; stw r8,12(r11)
//      stw r10,4(r11) ; stw r9,0(r11)
//
// The zeroing loop is f_arena_alloc.cpp's POINTER WALK with a precomputed
// end -- an indexed loop reloads both fields every iteration because the
// store `**q = 0` may alias the list.
//
// `blelr` is the guard written as a conditional RETURN, so the flip is the
// fall-through and is written first. The single `li r8,0` serves both the
// loop's store and the two zero stores in the flip.
//
// The first three flip stores come out at 16, 20 and 12, which is the order
// written here. THE SWAP'S TWO STORES ARE EMITTED IN REVERSE SOURCE ORDER:
// written `base = o; other = b;` they come out 4 then 0, and written the
// other way round they come out 0 then 4. That is MATCHED.md's second
// exception to "store order is source order" -- MSVC schedules a store
// across another store to the same object when the offsets are distinct --
// and it also decides the load order, because the first store's value takes
// r10. Both spellings of the swap through a single temporary give 0 then 4
// and are 4 words wrong; only the two named locals with the offset-0 store
// written first reproduce the image.

struct Arena
{
    /* 0x00 */ char* base;
    /* 0x04 */ char* other;
    /* 0x08 */ s32   size;
    /* 0x0C */ s32   cursor;
    /* 0x10 */ s32   unk0010;
    /* 0x14 */ s32   unk0014;
};
ASSERT_OFFSET(Arena, other,   0x04);
ASSERT_OFFSET(Arena, size,    0x08);
ASSERT_OFFSET(Arena, cursor,  0x0C);
ASSERT_OFFSET(Arena, unk0010, 0x10);
ASSERT_OFFSET(Arena, unk0014, 0x14);

struct FixupList
{
    /* 0x00 */ s32   count;
    /* 0x04 */ s32** slots;
};
ASSERT_OFFSET(FixupList, slots, 0x04);

extern Arena     g_arena;
extern FixupList g_fixups;

void ArenaFlip(void)
{
    int   c = g_fixups.count;
    s32** q = g_fixups.slots;
    s32** e = q + c;

    while (q != e)
    {
        **q = 0;
        ++q;
    }

    if (g_arena.cursor > 0)
    {
        g_arena.unk0010 = 0;
        g_arena.unk0014 = g_arena.cursor;
        g_arena.cursor  = 0;
        char* b = g_arena.base;
        char* o = g_arena.other;
        g_arena.base  = o;
        g_arena.other = b;
    }
}
