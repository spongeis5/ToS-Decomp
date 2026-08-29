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
// NOT MATCHED: 8 of 41 non-relocated words at /O2 /Os, 176 bytes against
// 180. The ONE missing instruction is `lwz r10,24(r3)` at 82772F3C, the
// reload of l->index, and everything after it is displaced by that word --
// which is why the score is 8 and not 40. The cause is a register choice:
// the target's materialised bool lands in r11, the register that held the
// index, so the index has to be reloaded; ours lands in r10 and leaves the
// index live.
//
// TWO NUMBERS IN THIS FILE WERE WRONG AND ARE WORTH THE CORRECTION.
//
// It said "44 of 45 words compared", which reads as a near-match and is not
// one: 44 is the COUNT of words compared, and the identical count is 8. A
// single displaced instruction moves every word after it, so "one missing
// word" and "eight words right" are the same measurement, and quoting the
// first without the second is how a stall comes to look finished.
//
// And `python tools/sweep.py --attempts` reports this row as `9 of 11`,
// which is not EmitSpan at all. sweep scores every function in the object
// and keeps the best; the inlined `OutOfRange` helper is emitted as its own
// 44-byte COMDAT, and 9 of its 11 words happen to agree with the first 11
// words of a 180-byte target. A helper left in a file therefore raises that
// file's reported score without any of it being about the function being
// attempted. The four EmitSpanB/C/D shape probes that used to sit here made
// it worse still, and have been deleted -- their measurements are below.
//
// SHAPE PROBES, all at /O2 /Os, sizes against the target's 180:
//
//     EmitSpan, the guard in a static bool helper   176 B, 8 of 41  <- this
//     the same with a two-level `Count(l)` helper   176 B, 8 of 41
//     `bool bad = OutOfRange(l);` named first       176 B, 8 of 41
//     the guard taking (index, runs)                176 B, 6 of 41
//     the guard taking (index, count)               176 B, 6 of 41
//     the index named in a local inside the guard   176 B, 6 of 41
//     the two `||` terms swapped                    172 B, 0 of 40
//     an int-returning guard instead of bool        160 B, 3 of 37
//     one unsigned test instead of the `||` pair    148 B, 1 of 34
//     `u32 pos = l->pos;` named first               168 B, 2 of 42
//
// The last three are the informative ones: they say the guard really is a
// two-term `||` returning `bool`, because collapsing it to one unsigned
// comparison or to an `int` loses the materialised 0/1 and 20 to 30 bytes
// with it. What no spelling reaches is which REGISTER that 0/1 lands in.
//
// THE NAMED-CONST-VIEW LEVER WAS THE RIGHT IDEA AND IT OVERSHOOTS. The
// target reloads `l->index` at 82772F3C with nothing stored in between,
// which is the signature that says the two reads were spelled differently
// (MATCHED.md, sub_821FF908), and a named `const Layout*` is now known to
// break exactly that tie (src/m_vector_reserve.cpp). Four placements were
// tried and none is closer:
//
//   * `const Layout* c = l;` driving the whole guard, written inline
//     instead of through the helper: 160 bytes. It does force the reload,
//     but it also destroys the MATERIALISED BOOL -- the guard stops
//     building 0/1 and branches straight out, so `li 0 / li 1 / clrlwi.`
//     disappear and with them four of the words that were already right.
//   * the same with only the index read viewed constly: 160 bytes, same.
//   * the const view INSIDE the helper: byte-identical to the baseline,
//     which is the "an inlined const accessor is folded" half of the lever.
//   * the const view on the BODY's index read instead of the guard's: also
//     byte-identical.
//
// So the reload is reachable but not while keeping the bool, and the bool is
// what makes the rest of the function right. That is a register-assignment
// choice, not a CSE one, and the const view reaches the CSE only.

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
