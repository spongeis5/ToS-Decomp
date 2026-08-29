// sub_827EE558 -- return a cached value when it is valid, else dispatch
// through a vtable. 52 bytes, 3 callers.
//
//      lwz    r11,428(r3) ; cmplwi cr6,r11,0 ; beq- cr6,<slow>
//      lbz    r10,24(r11) ; cmplwi r10,0 ; bne- <slow>
//      lwz    r3,16(r11) ; blr
//  slow:
//      lwz    r3,160(r3)
//      lwz    r11,0(r3) ; lwz r11,40(r11) ; mtctr r11 ; bctr
//
// `lwz ptr,0(obj) ; lwz ptr,n(ptr) ; mtctr ; bctr` is the virtual-call idiom
// and the slot index is 40 / 4 = 10. It is a TAIL call -- `bctr`, not
// `bctrl` -- so the dispatched result is this function's result.
//
// Both tests branch to the same slow path, which is the short-circuit form:
// `s != 0 && s->stale == 0`. Two separate `if`s would give the first its own
// exit.
//
// `/O2 /Os`, on two independent signatures at once. At /O2 the second
// `cmplwi` lands in CR6 where retail uses CR0, and the vtable slot gets a
// fresh `lwz r10,40(r11)` where retail reuses `lwz r11,40(r11)` -- four words,
// all of them register or CR-field names, with the instructions and their
// order already exact. That pairing is the sub_825E35E0 shape: /O2 gives the
// vtable slot a fresh register AND moves the compare to the other CR field,
// and the level moves both together.
//
// The tail dispatch reloads r3 from +160, so the object read for the vtable
// is a DIFFERENT member than the one tested, not an upcast of it.
//
// Nothing is relocated: 13 of 13 words are compared.

#include "types.h"

struct Cached
{
    /* 0x00 */ u8  unk0000[0x10];
    /* 0x10 */ s32 value;
    /* 0x14 */ u8  unk0014[4];
    /* 0x18 */ u8  stale;
};

ASSERT_OFFSET(Cached, value, 0x10);
ASSERT_OFFSET(Cached, stale, 0x18);

struct Source;

struct SourceVt
{
    /* 0x00 */ u8  unk0000[40];
    /* 0x28 */ int (*Compute)(Source*);
};

ASSERT_OFFSET(SourceVt, Compute, 40);

struct Source
{
    /* 0x00 */ SourceVt* vt;
};

struct Meter
{
    /* 0x000 */ u8      unk0000[0xA0];
    /* 0x0A0 */ Source* source;
    /* 0x0A4 */ u8      unk00A4[0x108];
    /* 0x1AC */ Cached* cache;
};

ASSERT_OFFSET(Meter, source, 160);
ASSERT_OFFSET(Meter, cache, 428);

int MeterValue(Meter* m)
{
    Cached* c = m->cache;

    if (c != 0 && c->stale == 0)
        return c->value;

    Source* s = m->source;
    return s->vt->Compute(s);
}
