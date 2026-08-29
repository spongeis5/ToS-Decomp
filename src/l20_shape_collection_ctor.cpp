// sub_826F99A0 -- hkpShapeCollection::hkpShapeCollection(type, collection
// type). 68 B, 4 callers.
//
// Named from RTTI, not invented.  build/vtables.txt:
//   8206C518  .?AVhkpShapeCollection@@     stored at +0
//   8206C4F8  .?AVhkpShapeCollection@@     stored at +16, second table
//   8206BC88  .?AVhkpShapeContainer@@      stored at +16 FIRST, then replaced
//
//      lis  r10,-32249
//      stw  r4,12(r3)
//      lis  r9,-32249 ; lis r8,-32249
//      li   r11,0 ; li r7,1
//      addi r6,r10,-17272     = 8206BC88
//      stw  r11,8(r3)
//      addi r4,r9,-15080      = 8206C518
//      sth  r7,6(r3)
//      addi r10,r8,-15112     = 8206C4F8
//      stw  r6,16(r3)         hkpShapeContainer's vptr
//      stw  r4,0(r3)          hkpShapeCollection's primary vptr
//      stw  r10,16(r3)        hkpShapeCollection's secondary vptr
//      stb  r11,20(r3)
//      stb  r5,21(r3)
//
// TWO VPTRS AND A DEAD ONE.  A class with two vtables and a second base at
// +16 is multiple inheritance, and hkpShapeCollection derives from both
// hkpShape and hkpShapeContainer -- so +16 is the hkpShapeContainer
// subobject.  Its base constructor writes ITS vptr there and the derived
// constructor immediately overwrites it, which is why the same offset is
// stored twice with the primary store in between.  That store the compiler
// would normally delete is the evidence for the base boundary, exactly as in
// src/h5_dsp_ctor.cpp.
//
// The two byte fields at +20 and +21 are hkpShapeCollection's own:
// m_disableWelding (always false here) and m_collectionType, the second
// parameter.  The word at +12 comes from the first parameter and +8 is
// zeroed; both are written before any vptr, so by the constructor rule they
// belong to a BASE -- hkpShape, whose user data and cached type they are.
//
// The three stores that precede the vptrs come out in reverse offset order
// (12, 8, 6) because three lis/addi pairs are in flight and MSVC fills the
// gaps with whichever store's value is ready; that is the documented
// scheduling exception to "store order is source order", so their emitted
// order is not read as source order here.

#include "types.h"

struct HkVtbl;
extern const HkVtbl kVT_hkpShapeCollection;       /* 8206C518 */
extern const HkVtbl kVT_hkpShapeCollection_2;     /* 8206C4F8 */
extern const HkVtbl kVT_hkpShapeContainer;        /* 8206BC88 */

struct hkpShapeContainer
{
    /* 0x00 */ const HkVtbl* vt;

    hkpShapeContainer() { vt = &kVT_hkpShapeContainer; }
};

struct hkpShape
{
    /* 0x00 */ const HkVtbl* vt;
    /* 0x04 */ u16           memSizeAndFlags;
    /* 0x06 */ u16           referenceCount;
    /* 0x08 */ u32           userData;
    /* 0x0C */ u32           type;

    hkpShape(u32 shapeType)
    {
        type          = shapeType;
        userData      = 0;
        referenceCount = 1;
    }
};

struct hkpShapeCollection : public hkpShape, public hkpShapeContainer
{
    /* 0x14 */ u8 disableWelding;
    /* 0x15 */ u8 collectionType;

    hkpShapeCollection(u32 shapeType, u8 collection);
};
ASSERT_OFFSET(hkpShapeCollection, disableWelding, 0x14);
ASSERT_OFFSET(hkpShapeCollection, collectionType, 0x15);

hkpShapeCollection::hkpShapeCollection(u32 shapeType, u8 collection)
  : hkpShape(shapeType),
    hkpShapeContainer()
{
    hkpShape::vt          = &kVT_hkpShapeCollection;
    hkpShapeContainer::vt = &kVT_hkpShapeCollection_2;
    disableWelding        = 0;
    collectionType        = collection;
}
