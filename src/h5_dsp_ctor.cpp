#include "types.h"

// sub_8253F5D8 -- constructor for a DSP module object. 192 B, 5 callers.
//
// STRUCTURAL: 26 distinct field offsets and TWO vtable stores to +0x00.
//
// WHAT THE DATA SAYS, read out of the image rather than guessed:
//
//   8205E630  "mod_dspi.cpp"           the module's own source-file string
//   8205E63C  00000000                 RTTI slot of the vtable at 8205E640
//   8205E640  8257F478                 vtable A -- one slot
//   8205E644  00000000                 RTTI slot of the vtable at 8205E648
//   8205E648  8257F478 8253C730 ...    vtable B -- slot 0 is A's slot 0
//   8205E348  472C4400 = 44100.0f      the sample rate
//   82002D40  3F800000 = 1.0f
//   82002DA4  00000000 = 0.0f
//   8296C5AC  a global holding a pointer
//
// Two vtables 8 bytes apart, sharing slot 0, with the derived one's RTTI slot
// zero: that is a BASE and a DERIVED class. Both vptrs are stored at +0x00 by
// this one function, so the base constructor is INLINED -- MSVC does not
// remove the redundant first store.
//
// THE LIST IDIOM, four times. 0x04, 0x2C, 0x38 and 0xB8 each get
// {this+off, this+off, 0}: a circular sentinel whose head and tail point at
// the sentinel itself. `addi r10,r3,44 ; stw r10,44(r3) ; stw r10,48(r3) ;
// stw r11,52(r3)` is the whole of it, and it is the same three instructions
// at each of the four offsets.
//
// ORDER. Member sub-objects are constructed in DECLARATION order, and the
// emitted order 0x18, then the lists at 0x2C, 0x38, 0xB8, is exactly that --
// so `m_source` is a member initialiser and the three lists follow it in
// declaration order with nothing chosen by hand.
//
// The head of the function looks out of order and is not: 0x0C is stored
// before 0x04 and 0x08, and both vptr stores land after all four base-class
// stores. Both are MATCHED.md's documented exception -- a cheap store is
// scheduled into the gap while a `lis`/`addi` address computation is in
// flight, and forming 8205E640 takes `lis`+`addi` while `r11 = 0` is ready
// immediately.
//
// WHY 0x18 IS A **BASE** MEMBER, and this is the whole function. Written with
// the global read in the derived class, MSVC DELETES the base vptr store as
// dead -- one `lis`, one `addi` and one `stw` gone, 180 bytes against 192,
// and the object then holds a single vtable reference. Nothing between the
// two stores of a constant to +0x00 can be proven to read it, so DSE is
// entitled to drop the first.
//
// The image says where the read goes: `lwz r8,-14932(r8)` is emitted BETWEEN
// the two vptr stores. A load from a global cannot be disambiguated from
// `*(void**)this` -- `this` is a parameter and could point anywhere -- so a
// global read sitting between them makes the first store live again. That
// only happens if the read is in the BASE constructor, because every base
// member initialiser runs before the derived vptr is stored.
//
// So the base runs to 0x1C, and this is a layout fact recovered from a
// dead-store elimination, which is not a way it is usually possible to learn
// one. MEASURED: 0x18 in the derived is 180 bytes and 0 of 45 words; 0x18 in
// the base is what is written here.
//
// THE BODY IS TWO STREAMS. MATCHED.md's rule from sub_8214CCB8 held exactly:
// the first attempt's integer stores came out in its own source order
// (0x44, 0x48, 0x4C, 0x54, 0x56, 0x100, 0x104, 0x110) and its float stores
// in its own (0xF4, 0xF8, 0xFC), and the SCHEDULER produced the merge
// f,f,i,f,i,i,i,i,i,i,i -- which is the target's merge, one for one. So the
// merge carries no information and the two orders do: retail's integer
// stream is 0x104, 0x4C, 0x110, 0x44, 0x48, 0x54, 0x56, 0x100, and that is
// the order written below.

struct DspLink;

// The sentinel. `head = tail = (DspLink*)this` is what makes the three
// stores read as this+off rather than as a pointer that came from anywhere.
struct DspList
{
    /* 0x00 */ DspLink* head;
    /* 0x04 */ DspLink* tail;
    /* 0x08 */ s32      count;

    DspList()
    {
        head  = (DspLink*)this;
        tail  = (DspLink*)this;
        count = 0;
    }
};
ASSERT_OFFSET(DspList, head, 0x00);
ASSERT_OFFSET(DspList, tail, 0x04);
ASSERT_OFFSET(DspList, count, 0x08);
ASSERT_SIZE(DspList, 0x0C);

struct DspSource;
extern DspSource* g_dsp_source_8296C5AC;

// THE SENTINEL AND THE ID SIT IN A BASE OF THE BASE, and that is what puts
// their four stores AHEAD of the first vptr store.
//
// A constructor stores its own vptr before any member initialiser, so nothing
// a class initialises itself can be emitted before its vptr. Retail's order
// is 0x04, 0x08, 0x0C, 0x10, then vptr A -- so those four are not DspBase's
// members. They belong to a base that DspBase derives from, because MSVC runs
// BASE constructors before storing the vptr.
//
// MEASURED: with them declared in DspBase the vptr A store schedules fifth
// from the top instead of thirteenth and eleven words are displaced; with
// them in a base of it, nothing moves. MSVC lays a non-polymorphic base out
// AFTER the vptr it adds, which is why list04 lands at 0x04 and not 0x00 --
// ASSERT_OFFSET below is what actually checks that.
struct DspRoot
{
    /* 0x04 */ DspList list04;
    /* 0x10 */ s32     id10;

    DspRoot() : id10(-1) {}
};

struct DspBase : public DspRoot
{
    /* 0x00 */                          // vptr -- 8205E640
    /* 0x14 */ u32        unk0014;
    /* 0x18 */ DspSource* source18;

    DspBase() : source18(g_dsp_source_8296C5AC) {}
    virtual void Slot0();
};
ASSERT_OFFSET(DspBase, list04, 0x04);
ASSERT_OFFSET(DspBase, id10, 0x10);
ASSERT_OFFSET(DspBase, source18, 0x18);
ASSERT_SIZE(DspBase, 0x1C);

struct DspModule : public DspBase
{
    /* 0x1C */ char       unk001C[0x10];
    /* 0x2C */ DspList    list2C;
    /* 0x38 */ DspList    list38;
    /* 0x44 */ s32        i44;
    /* 0x48 */ s32        i48;
    /* 0x4C */ s32        i4C;
    /* 0x50 */ u32        unk0050;
    /* 0x54 */ u16        h54;
    /* 0x56 */ u16        h56;
    /* 0x58 */ char       unk0058[0x60];
    /* 0xB8 */ DspList    listB8;
    /* 0xC4 */ char       unk00C4[0x30];
    /* 0xF4 */ f32        gainF4;
    /* 0xF8 */ f32        rateF8;
    /* 0xFC */ f32        fFC;
    /* 0x100 */ s32       i100;
    /* 0x104 */ s32       i104;
    /* 0x108 */ char      unk0108[0x08];
    /* 0x110 */ s32       i110;

    DspModule();
    virtual void Slot0();
    virtual void Slot1();
};
ASSERT_OFFSET(DspModule, list2C, 0x2C);
ASSERT_OFFSET(DspModule, list38, 0x38);
ASSERT_OFFSET(DspModule, i44, 0x44);
ASSERT_OFFSET(DspModule, i4C, 0x4C);
ASSERT_OFFSET(DspModule, h54, 0x54);
ASSERT_OFFSET(DspModule, h56, 0x56);
ASSERT_OFFSET(DspModule, listB8, 0xB8);
ASSERT_OFFSET(DspModule, gainF4, 0xF4);
ASSERT_OFFSET(DspModule, rateF8, 0xF8);
ASSERT_OFFSET(DspModule, fFC, 0xFC);
ASSERT_OFFSET(DspModule, i100, 0x100);
ASSERT_OFFSET(DspModule, i104, 0x104);
ASSERT_OFFSET(DspModule, i110, 0x110);

DspModule::DspModule()
{
    i104 = 0;
    i4C  = 0;
    i110 = 0;
    i44  = 0;
    i48  = 0;
    h54  = (u16)-1;
    h56  = 0;
    i100 = 128;

    gainF4 = 1.0f;
    rateF8 = 44100.0f;
    fFC    = 0.0f;
}
