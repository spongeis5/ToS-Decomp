#include "types.h"

// sub_82703E28 -- constructor: vtable, a dozen scalar fields, a zeroed
// 16-byte sub-object, and an embedded 516-byte block whose cursor and
// remaining count are computed from the block's own address.
// 124 B, 15 callers.  /O2 /Os.
//
//      addi    r10,r3,240       d = blk.data
//      lis     r9,-32248
//      addi    r8,r10,3
//      addi    r7,r9,-11300     &kVTable_8207D3DC
//      rlwinm  r9,r8,0,0,29     (d + 3) & ~3
//      li      r11,0
//      stw     r7,0(r3)
//      addi    r9,r9,4          p = aligned + 4
//      li      r6,-1
//      stb     r11,4(r3)
//      sth     r11,6(r3)
//      subf    r10,r9,r10       d - p
//      sth     r6,8(r3)
//      stw     r11,12(r3)
//      stw     r11,16(r3)
//      lwz     r8,0(r4)   ; stw r8,20(r3)
//      addi    r8,r10,516       (d - p) + 516
//      lwz     r7,4(r4)   ; stw r7,24(r3)
//      addi    r10,r3,32        DEAD -- r10 is reloaded on the next line
//      lwz     r10,8(r4)  ; stw r10,28(r3)
//      stw     r11,32(r3) ; 36 ; 40 ; 44
//      stw     r11,240(r3)
//      stw     r9,756(r3)
//      stw     r8,760(r3)
//      blr
//
// Three separate things had to be right.
//
// 1. `addi r10,r3,32` computes an address that is immediately overwritten and
//    never used. That is the fingerprint of an INLINED HELPER whose pointer
//    argument was materialised and then folded away at every use -- the same
//    leftover MSVC produces for `Init(&s->g[1])` in sub_82164040. So the four
//    zeroes at 32..44 are a sub-object being cleared, not four fields, and
//    the clear needs TWO levels of inlining to leave the dead address behind:
//    a flat `q->a = q->b = q->c = q->d = 0` helper is 120 bytes with no
//    leftover, and so are memset, a member function, and a counted loop.
//
// 2. NAMING THE BLOCK'S BASE POINTER IN A LOCAL is what fixes the prologue
//    schedule, and it is worth two words of ordering plus a register. Spelled
//    `b->data` at all three uses -- the header store, the alignment and the
//    difference -- MSVC aligns in place (`addi r9,r10,3 ; rlwinm r9,r9,...`)
//    and sinks the vtable store to 11th; with `char* d = o->blk.data;` it
//    aligns through a second register exactly as the target does and the
//    vtable store comes out 7th. Nine other spellings of the same arithmetic
//    -- pointer-first `+3`, the `+3` in its own local, an AlignUp helper, a
//    whole-expression helper, unsigned throughout, both values named before
//    either store -- are all 18 of 29 and identical to each other.
//
// 3. /Os, again by the register-coalescing signature: /O2 gives 13 of 29 with
//    the same instructions and a different prologue order.
//
// The block is 516 bytes with a 4-byte header word: the cursor is the
// 4-aligned base plus 4, and the remaining count is 516 less the distance
// from the base to it.

struct VT82703E28;
extern const VT82703E28 kVTable_8207D3DC;

struct BlkSrc
{
    /* 0x00 */ s32 a;
    /* 0x04 */ s32 b;
    /* 0x08 */ s32 c;
};

struct BlkQuad
{
    /* 0x00 */ s32 a;
    /* 0x04 */ s32 b;
    /* 0x08 */ s32 c;
    /* 0x0C */ s32 d;
};

struct Blk
{
    /* 0x000 */ char  data[516];
    /* 0x204 */ char* cur;
    /* 0x208 */ s32   left;
};
ASSERT_OFFSET(Blk, cur,  0x204);
ASSERT_OFFSET(Blk, left, 0x208);

struct BlkOwner
{
    /* 0x000 */ const VT82703E28* vt;
    /* 0x004 */ u8      f004;
    /* 0x005 */ u8      unk0005;
    /* 0x006 */ u16     f006;
    /* 0x008 */ s16     f008;
    /* 0x00A */ u16     unk000A;
    /* 0x00C */ s32     f00C;
    /* 0x010 */ s32     f010;
    /* 0x014 */ s32     f014;
    /* 0x018 */ s32     f018;
    /* 0x01C */ s32     f01C;
    /* 0x020 */ BlkQuad quad;
    /* 0x030 */ char    unk0030[0x0F0 - 0x030];
    /* 0x0F0 */ Blk     blk;
};
ASSERT_OFFSET(BlkOwner, f014, 0x014);
ASSERT_OFFSET(BlkOwner, quad, 0x020);
ASSERT_OFFSET(BlkOwner, blk,  0x0F0);

static void ZeroPair(s32* p)
{
    p[0] = 0;
    p[1] = 0;
}

static void QuadInit(BlkQuad* q)
{
    ZeroPair(&q->a);
    ZeroPair(&q->c);
}

void BlkOwnerConstruct(BlkOwner* o, const BlkSrc* s)
{
    o->vt   = &kVTable_8207D3DC;
    o->f004 = 0;
    o->f006 = 0;
    o->f008 = -1;
    o->f00C = 0;
    o->f010 = 0;
    o->f014 = s->a;
    o->f018 = s->b;
    o->f01C = s->c;
    QuadInit(&o->quad);

    char* d = o->blk.data;
    *(s32*)d = 0;
    char* p = (char*)(((u32)d + 3) & ~3u) + 4;
    o->blk.cur  = p;
    o->blk.left = (s32)(d - p) + 516;
}
