// sub_821A4628 -- three stores, 28 bytes, 108 callers.
//
//      lis     r10,-32256
//      li      r11,0
//      addi    r9,r10,19716
//      stw     r11,8(r3)       +8  = 0
//      stw     r9,0(r3)        +0  = &kVTable
//      stw     r11,12(r3)      +12 = 0
//      blr
//
// The store ORDER is 8, 0, 12 -- not field order. Written here in that same
// order, on the reading that the compiler emitted the source's own sequence
// and interleaved the address computation ahead of it.
//
// This one has independent stores, so it is the shape that has resisted
// before; attempted anyway because 108 callers is worth one try.

struct VTable;
extern const VTable kVTable;

struct Object
{
    const VTable* vt;           // +0x00
    int           pad04;        // +0x04
    int           a;            // +0x08
    int           b;            // +0x0C
};

void Construct(Object* o)
{
    o->a  = 0;
    o->vt = &kVTable;
    o->b  = 0;
}
