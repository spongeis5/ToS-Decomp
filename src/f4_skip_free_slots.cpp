// sub_8280D210 -- advance a cursor past entries whose state is -2.
// 112 B, 5 callers.  NEEDS /O2 /Os.
//
//   lwz r11,0(r3) ; lwz r10,4(r3)
//   lwz r9,0(r11) ; lwz r9,4(r9)
//   cmpw cr6,r10,r9 ; bgtlr cr6            the guard, SIGNED
//   addi r10,r10,1 ; stw r10,4(r3)
//   lwz r9,0(r11) ; lwz r9,4(r9)
//   cmplw cr6,r10,r9 ; bgtlr cr6           the peeled loop test, UNSIGNED
//   <body> ; addi ; stw ; <reload the whole chain> ; ble+ back
//
// The increment sits between the guard and the peeled test, and the body's
// own increment is at the bottom, which is MSVC's rotation of a plain
// `while` with the increment written last inside it. A `do/while` with the
// second test written as its own `if` is the same code on paper and is NOT
// the same code here: at /Os the compiler tail-merges that shape's two
// increments and branches into the loop, 80 B against 112.
//
// `mulli r9,r10,48` IS THE OPTIMISATION LEVEL, not the source. Ten shapes
// were compiled at both levels -- subscript with an `int` index, with an
// `unsigned` index, with the index loaded from a struct, `(char*)e + i * 48`,
// `e + i`, `i * sizeof(Entry)`, the offset in its own local, a 12-`int`
// element, an `__int64` index, and the whole loop -- and every one of the
// ten expands to `rlwinm`/`add`/`rlwinm` at /O2 and emits `mulli ...,48` at
// /O2 /Os. There is no source shape behind this word.
//
// The level also produces the two reloads the /O2 code CSEs away: `o->entries`
// inside the loop and `c->owner` at the loop bottom, though r11 still holds
// the latter and is used a few instructions earlier. Fourteen shapes were
// measured at both levels; the best at /O2 is 4 of 28 and three different
// shapes reach 28 of 28 at /O2 /Os, so nothing about the naming is being
// claimed here beyond "spelled out works".
//
// The first compare is `cmpw` and the other two are `cmplw` on the same two
// fields, which is MATCHED.md's signedness split. The casts are written out
// because the bytes require them, not because the shape is comfortable.
//
// `mulli r9,r10,48` pins the element size, and `lwz r9,4(r9)` reading the
// count off the same base the elements are indexed from puts the count in
// element ZERO's second word -- slot 0 is the header, and indices run from 1.

#include "types.h"

struct Entry
{
    /* 0x00 */ int unk0000;
    /* 0x04 */ int count;
    /* 0x08 */ int state;
    /* 0x0C */ u8  unk000C[0x24];
};

ASSERT_SIZE(Entry, 48);
ASSERT_OFFSET(Entry, count, 4);
ASSERT_OFFSET(Entry, state, 8);

struct Owner
{
    /* 0x00 */ Entry* entries;
};

struct Cursor
{
    /* 0x00 */ Owner* owner;
    /* 0x04 */ int    index;
};

ASSERT_OFFSET(Cursor, index, 4);

void CursorSkipFree(Cursor* c)
{
    if (c->index > c->owner->entries->count)
        return;

    c->index = c->index + 1;

    while ((u32)c->index <= (u32)c->owner->entries->count)
    {
        if (c->owner->entries[c->index].state != -2)
            return;
        c->index = c->index + 1;
    }
}
