// sub_827007E8 -- store a fixed address into the first field. 16 B, 32 callers.
//
//      lis     r11,-32248
//      addi    r11,r11,-11828
//      stw     r11,0(r3)
//      blr
//
// The lis/addi pair is one relocated symbol reference, so 2 of 4 words are
// masked and the evidence here is correspondingly weaker -- the shape is
// confirmed, the address is not.

struct VTable;
extern const VTable kVTable;

struct Object { const VTable* vt; };

void SetVTable(Object* o)
{
    o->vt = &kVTable;
}
