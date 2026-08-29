#include "types.h"

// sub_8267ACC0 -- constructor of the class whose vtable is 8206BE70, which
// MSVC RTTI names `.?AVhkpWorld@@`. 236 B.
//
// IN SCOPE, and the project used to say otherwise. Havok is 341,792 bytes of
// this image and there is no archive to link; identifying it by RTTI says
// what it is, it does not make it go away. See the attribution table in
// README.md -- 32.1% of .text can be linked from XDK libraries we hold, and
// the other 67.9% has to be written, Havok included.
//
// IT IS ALSO EASIER THAN GAME CODE, not harder. The class name came free
// from RTTI, Havok 6.5's API is publicly documented, and the member type
// below identifies itself from three constants.
//
// SIXTEEN hkArrays. Every 12-byte triple is `{0, 0, 0x80000000}`:
//
//      stw r11,40(r3)     data = 0
//      stw r11,44(r3)     size = 0
//      stw r10,48(r3)     capacityAndFlags = 0x80000000
//
// which is hkArray's empty state -- a pointer, a count, and a capacity whose
// top bit means DON'T DEALLOCATE. Three at 0x28 and thirteen at 0x108.
//
// **That is the same packed word as src/m_vector_reserve.cpp**, matched
// earlier from the other end of the program: `{void* data; s32 count; s32
// capacityAndFlags;}` with bit 31 meaning "this buffer is not ours to free"
// and the low 30 bits the capacity. So VectorReserve and VectorGrow are
// hkArray's growth path, and this constructor is sixteen of the same type.
// Two functions matched a day apart, from opposite ends, turn out to be the
// same container.
//
// THE VTABLE STORE IS FIRST, which src/m_ctor_94.cpp could not achieve as a
// free function in any source order. A real C++ constructor plants the vptr
// before any user code, and that is what makes it move.
struct hkpWorldVT;
extern const hkpWorldVT kVTable_8206BE70;

// Havok's array: data, size, and a capacity whose top bit is the
// don't-deallocate flag. 0x80000000 with a zero capacity is the empty,
// nothing-owned state.
struct hkArray
{
    void* m_data;
    s32   m_size;
    s32   m_capacityAndFlags;

    void Init()
    {
        m_data = 0;
        m_size = 0;
        m_capacityAndFlags = (s32)0x80000000;
    }
};
ASSERT_SIZE(hkArray, 12);

struct hkpWorld
{
    /* 0x000 */ /* vptr */
    /* 0x004 */ char    unk0004[2];
    /* 0x006 */ u16     m_flags06;
    /* 0x008 */ char    unk0008[0x28 - 0x08];
    /* 0x028 */ hkArray m_a0;
    /* 0x034 */ hkArray m_a1;
    /* 0x040 */ hkArray m_a2;
    /* 0x04C */ char    unk004C[0xA4 - 0x4C];
    /* 0x0A4 */ s32     m_fA4;
    /* 0x0A8 */ u16     m_fA8;
    /* 0x0AA */ char    unk00AA[0x108 - 0xAA];
    /* 0x108 */ hkArray m_b00;
    /* 0x114 */ hkArray m_b01;
    /* 0x120 */ hkArray m_b02;
    /* 0x12C */ hkArray m_b03;
    /* 0x138 */ hkArray m_b04;
    /* 0x144 */ hkArray m_b05;
    /* 0x150 */ hkArray m_b06;
    /* 0x15C */ hkArray m_b07;
    /* 0x168 */ hkArray m_b08;
    /* 0x174 */ hkArray m_b09;
    /* 0x180 */ hkArray m_b10;
    /* 0x18C */ hkArray m_b11;
    /* 0x198 */ hkArray m_b12;

    hkpWorld();
    virtual ~hkpWorld();
};
ASSERT_OFFSET(hkpWorld, m_flags06, 0x06);
ASSERT_OFFSET(hkpWorld, m_a0, 0x28);
ASSERT_OFFSET(hkpWorld, m_fA4, 0xA4);
ASSERT_OFFSET(hkpWorld, m_b00, 0x108);
ASSERT_OFFSET(hkpWorld, m_b12, 0x198);

hkpWorld::hkpWorld()
{
    m_flags06 = 1;
    m_a0.Init();
    m_a1.Init();
    m_a2.Init();
    m_fA4 = -47;
    m_fA8 = 0;
    m_b00.Init();
    m_b01.Init();
    m_b02.Init();
    m_b03.Init();
    m_b04.Init();
    m_b05.Init();
    m_b06.Init();
    m_b07.Init();
    m_b08.Init();
    m_b09.Init();
    m_b10.Init();
    m_b11.Init();
    m_b12.Init();
}
