#include "types.h"

// sub_82772F08 -- pick which span of a UTF-16 buffer to hand to the emitter,
// then tail call it three different ways. 180 B, 7 callers.  /O2 /Os.
//
//      lwz     r11,24(r3) ; cmpwi cr6,r11,0 ; blt- -> set 1
//      lwz     r10,20(r3) ; lwz r10,4(r10)
//      cmplw   cr6,r11,r10 ; li r11,0 ; blt- -> keep 0
//      li      r11,1
//      lwz     r5,32(r3)                     l->pos, AFTER the bool is built
//      clrlwi. r11,r11,24 ; bne- -> C
//   A/B:
//      lwz     r11,20(r3) ; lwz r10,24(r3) ; mulli r10,r10,12
//      lwz     r11,0(r11) ; add r10,r10,r11 ; lwz r11,0(r10)
//      cmplw   cr6,r5,r11 ; bge- -> B
//   A:  lwz r9,28(r3) ; rotlwi r10,r5,0 ; li r7,0 ; subf r6,r10,r11
//       rlwinm r10,r10,1 ; lwz r11,0(r9) ; add r4,r10,r11 ; b 82770F60
//   B:  lwz r8,28(r3) ; rlwinm r9,r11,1 ; mr r5,r11 ; lwz r7,8(r10)
//       lwz r6,4(r10) ; lwz r11,0(r8) ; add r4,r9,r11 ; b 82770F60
//   C:  lwz r11,28(r3) ; li r7,0 ; lwz r9,32(r3) ; rlwinm r10,r9,1
//       lwz r8,4(r11) ; lwz r11,0(r11) ; subf r6,r9,r8
//       add r4,r10,r11 ; b 82770F60
//
// `li 0 / li 1 / clrlwi. / bne-` is a materialised-then-masked bool, i.e. an
// inlined bool-returning helper; both of its guards share the `li r11,1`,
// which is the `||` spelling rather than two `if`s.
//
// `rlwinm rX,rY,1,0,30` scales by TWO before adding the buffer base, so the
// buffer is 16-bit. `subf rD,rA,rB` is rB - rA, so both subtractions read
// `end - pos` in source order.
//
// THE MULTIPLY IS THE FLAG. At /O2 the 12-byte stride comes out as the
// shift-add chain `rlwinm 1 / add / rlwinm 2`; at /O2 /Os it is a single
// `mulli r10,r10,12`, which is what the target has. That is a new instance of
// the /Os signature -- size-directed strength reduction rather than register
// coalescing -- and it is worth two words on its own.
//
// MEASUREMENT (see the probes below): the best shape is EmitSpan at /O2 /Os,
// 176 bytes against the target's 180 and 44 of 45 words compared. Every
// instruction is right; the ONE missing word is `lwz r10,24(r3)` at 82772F3C,
// the reload of l->index. The target reloads it because its bool lands in r11
// -- the register that held the index -- while ours lands in r10 and leaves
// the index live, so no reload is needed. That is a register-assignment
// choice, and the register numbers are the only thing wrong afterwards.
//   EmitSpanB (`bool bad` named first)     : see probe
//   EmitSpanC (`u32 pos` named first)      : 168 B, the pos load hoisted to
//                                            the second instruction, 2 of 42

struct Run
{
    /* 0x00 */ u32 start;
    /* 0x04 */ u32 f04;
    /* 0x08 */ u32 f08;
};
ASSERT_SIZE(Run, 12);

struct RunList
{
    /* 0x00 */ Run* runs;
    /* 0x04 */ u32  count;
};
ASSERT_OFFSET(RunList, count, 0x04);

struct Text
{
    /* 0x00 */ u16* chars;
    /* 0x04 */ u32  length;
};
ASSERT_OFFSET(Text, length, 0x04);

struct Layout
{
    /* 0x00 */ char     unk0000[0x14];
    /* 0x14 */ RunList* runs;
    /* 0x18 */ s32      index;
    /* 0x1C */ Text*    text;
    /* 0x20 */ u32      pos;
};
ASSERT_OFFSET(Layout, runs,  0x14);
ASSERT_OFFSET(Layout, index, 0x18);
ASSERT_OFFSET(Layout, text,  0x1C);
ASSERT_OFFSET(Layout, pos,   0x20);

int Emit(Layout* l, u16* p, u32 from, u32 n, u32 extra);

static bool OutOfRange(Layout* l)
{
    return l->index < 0 || (u32)l->index >= l->runs->count;
}

int EmitSpan(Layout* l)
{
    if (!OutOfRange(l))
    {
        Run* r = &l->runs->runs[l->index];
        if (l->pos < r->start)
            return Emit(l, l->text->chars + l->pos, l->pos,
                        r->start - l->pos, 0);
        return Emit(l, l->text->chars + r->start, r->start, r->f04, r->f08);
    }
    return Emit(l, l->text->chars + l->pos, l->pos,
                l->text->length - l->pos, 0);
}

/* ---- shape probes; pick with --sym ---- */

int EmitSpanB(Layout* l)
{
    bool bad = OutOfRange(l);

    if (!bad)
    {
        Run* r = &l->runs->runs[l->index];
        if (l->pos < r->start)
            return Emit(l, l->text->chars + l->pos, l->pos,
                        r->start - l->pos, 0);
        return Emit(l, l->text->chars + r->start, r->start, r->f04, r->f08);
    }
    return Emit(l, l->text->chars + l->pos, l->pos,
                l->text->length - l->pos, 0);
}

int EmitSpanC(Layout* l)
{
    bool bad = OutOfRange(l);
    u32  pos = l->pos;

    if (!bad)
    {
        Run* r = &l->runs->runs[l->index];
        if (pos < r->start)
            return Emit(l, l->text->chars + pos, pos, r->start - pos, 0);
        return Emit(l, l->text->chars + r->start, r->start, r->f04, r->f08);
    }
    return Emit(l, l->text->chars + pos, pos, l->text->length - pos, 0);
}

int EmitSpanD(Layout* l)
{
    if (OutOfRange(l))
        return Emit(l, l->text->chars + l->pos, l->pos,
                    l->text->length - l->pos, 0);

    {
        Run* r = &l->runs->runs[l->index];
        if (l->pos < r->start)
            return Emit(l, l->text->chars + l->pos, l->pos,
                        r->start - l->pos, 0);
        return Emit(l, l->text->chars + r->start, r->start, r->f04, r->f08);
    }
}
