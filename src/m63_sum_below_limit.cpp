// sub_822DF630 -- return 0 unless two counters still fit under a limit, and
// otherwise tail-call with a global's address. 52 bytes, 3 callers.
//
//      lwz  r10,20(r3) ; lwz r11,4(r3) ; lwz r9,32(r3)
//      add  r8,r10,r11
//      cmpw cr6,r8,r9 ; blt- cr6,<call>
//      li   r3,0 ; blr
//  call:
//      lis  r11,-32102 ; li r5,14 ; addi r3,r11,6492 ; li r4,24
//      b    0x82609740
//
// `blt-` jumping FORWARD to the call means the zero return is the
// fall-through, so the guard is written first and positively: the failing
// path is the one the source states.
//
// `addi` off a relocated `lis` with no load is the ADDRESS of a global. It is
// not a string -- the 32 bytes at 829A195C are all zero, checked in the image
// rather than assumed from the shape.
//
// `add r8,r10,r11` puts the +20 field in rA. rA takes the operand whose
// SOURCE read comes later, so the sum is written with +4 first even though
// +20 is the load MSVC issues first.
//
// `cmpw` is signed: both counters and the limit are ints.
//
// 2 of 13 words are relocated.

#include "types.h"

struct Budget
{
    /* 0x00 */ u8  unk0000[4];
    /* 0x04 */ s32 used;
    /* 0x08 */ u8  unk0008[0x0C];
    /* 0x14 */ s32 pending;
    /* 0x18 */ u8  unk0018[0x08];
    /* 0x20 */ s32 limit;
};

ASSERT_OFFSET(Budget, pending, 20);
ASSERT_OFFSET(Budget, limit, 32);

struct Arena;

// THE SECOND ARENA, and the address is in the name because there are two.
//
// This site resolves to 829A195C. `src/f_arena_alloc.cpp`, its twin, and
// `src/y1_arena_flip.cpp` all name an `Arena` at 82A35288. Both are real and
// both are in `.data`; the game has two of these objects, and calling them
// both `g_arena` made build.py report
//
//     WOULD NOT LINK: ?g_arena@@3UArena@@A
//         829A195C  referenced from 822DF658
//         82A35288  referenced from 82606ED0, 82606FA4
//
// which is exactly what that check is for: every byte still verified,
// because each site is patched from the image independently, and one name
// still cannot have two addresses. The bytes were right and the source model
// was wrong.
//
// Renaming does not change codegen -- the name is a relocation target, not
// an operand -- so this still matches. The address-qualified form is the
// same convention `kVTable_827007E8` and `kFloat_<addr>` already use.
extern Arena g_arena_829A195C;

int ArenaTake(Arena* a, int size, int align);

int TakeIfRoom(Budget* b)
{
    if (b->used + b->pending >= b->limit)
        return 0;

    return ArenaTake(&g_arena_829A195C, 24, 14);
}
