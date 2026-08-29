// sub_826FE5B8 and sub_826FE5C8 -- two objects, each storing a fixed vtable
// pointer into its first field. 16 bytes each, 2 callers each.
//
//      lis     r11,<hi>
//      addi    r11,r11,<lo>
//      stw     r11,0(r3)
//      blr
//
// They are CONTIGUOUS -- 826FE5B8 + 16 = 826FE5C8, no gap at all -- so they
// are two functions of one translation unit, which is why they live in one
// file.
//
// They reference DIFFERENT vtables, 16 bytes apart in .rdata, and the names
// carry the address because nothing yet says what these classes are. That
// matters: an earlier version of this file used a single `kVTable` for both
// sites and for src/ctor_vt.cpp as well, so one symbol resolved to THREE
// addresses. Every byte still verified -- build.py patches each site from the
// image independently -- but a real linker gives one name one address, and
// the model was wrong while the bytes were right. build.py now refuses to let
// that pass silently.
//
// Reading the vtables confirms they are siblings:
//
//   8207D170:  826FF978  8262FD88  8262FD90  ...
//   8207D180:  826FF9E0  8262FD88  8262FD90  "filterInfo"
//
// Same two inherited entries, different first slot -- two related classes.
// The string immediately after the second vtable is also why the addresses
// are known to be exactly right: a wrong one would not land on that boundary.
//
// Only 2 of 4 words are non-relocated, so the shape is confirmed and the
// addresses are not independently attested by the comparison itself.

#include "types.h"

struct VTable;

extern const VTable kVTable_8207D170;
extern const VTable kVTable_8207D180;

struct Object
{
    /* 0x00 */ const VTable* vt;
};

ASSERT_OFFSET(Object, vt, 0x00);

// sub_826FE5B8
void SetVTableD170(Object* o)
{
    o->vt = &kVTable_8207D170;
}

// sub_826FE5C8
void SetVTableD180(Object* o)
{
    o->vt = &kVTable_8207D180;
}
