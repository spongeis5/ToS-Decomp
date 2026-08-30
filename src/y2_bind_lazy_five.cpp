// sub_82232420 -- the sibling of src/y2_bind_lazy_six.cpp (sub_822306D8),
// 0xE2B8 earlier and binding five of the same lazily-constructed singletons
// plus one that is published TWICE. 324 bytes, no direct callers.
//
// Four of its five objects are shared with sub_822306D8 -- 829AC320,
// 829AC328, 829AC330 and 829AC348, with the same guards and the same stored
// heads -- so the two functions bind overlapping sets and the externs here
// deliberately carry the same names as there.
//
//      829AC320 / 829AC324 / 8200BA94  ->  +32
//      829AC328 / 829AC32C / 8200BA98  ->  +28
//      829AC330 / 829AC334 / 8200BA9C  ->  +40
//      829AC348 / 829AC34C / 8200BAC0  ->  +44
//      829AC358 / 829AC35C / 8200BAF0  ->  +52  AND  +56
//
// THE LAST OBJECT IS GUARD-CHECKED TWICE, once for each field, and MSVC did
// not prove the second check redundant even though the first block's own
// fall-through establishes the bit. That is what says the source publishes
// the same accessor twice rather than reusing a local.
//
// The two repeated blocks also come out with the stores in the OPPOSITE
// order from the first four -- object first, guard second -- because the
// second occurrence lets MSVC hoist the shared `lis` for the guard base and
// the shared `addi` for the stored head out of both blocks, and the
// scheduler then fills differently. Nothing in the source changes.

#include "types.h"

struct LazySingleton
{
    const void* head;
};

extern LazySingleton g_lazy1;       /* 829AC320 */
extern u32           g_lazy1Guard;  /* 829AC324 */
extern const void*   g_lazy1Head;   /* 8200BA94 */

extern LazySingleton g_lazy2;       /* 829AC328 */
extern u32           g_lazy2Guard;  /* 829AC32C */
extern const void*   g_lazy2Head;   /* 8200BA98 */

extern LazySingleton g_lazy3;       /* 829AC330 */
extern u32           g_lazy3Guard;  /* 829AC334 */
extern const void*   g_lazy3Head;   /* 8200BA9C */

extern LazySingleton g_lazy6;       /* 829AC348 */
extern u32           g_lazy6Guard;  /* 829AC34C */
extern const void*   g_lazy6Head;   /* 8200BAC0 */

extern LazySingleton g_lazy8;       /* 829AC358 */
extern u32           g_lazy8Guard;  /* 829AC35C */
extern const void*   g_lazy8Head;   /* 8200BAF0 */

struct HandlerSlot2
{
    /* 0x00 */ void* unk0000;
    /* 0x04 */ void* handler;
};
ASSERT_OFFSET(HandlerSlot2, handler, 0x04);

extern HandlerSlot2 g_handlerSlot2;  /* 82955D58 */

void BoundHandler2();                /* 82204B90 */

struct Binder2
{
    /* 0x00 */ u8             unk0000[0x18];
    /* 0x18 */ u8             ready;
    /* 0x19 */ u8             unk0019[0x03];
    /* 0x1C */ LazySingleton* p1C;
    /* 0x20 */ LazySingleton* p20;
    /* 0x24 */ u8             unk0024[0x04];
    /* 0x28 */ LazySingleton* p28;
    /* 0x2C */ LazySingleton* p2C;
    /* 0x30 */ u8             unk0030[0x04];
    /* 0x34 */ LazySingleton* p34;
    /* 0x38 */ LazySingleton* p38;
};
ASSERT_OFFSET(Binder2, ready, 0x18);
ASSERT_OFFSET(Binder2, p1C,   0x1C);
ASSERT_OFFSET(Binder2, p20,   0x20);
ASSERT_OFFSET(Binder2, p28,   0x28);
ASSERT_OFFSET(Binder2, p2C,   0x2C);
ASSERT_OFFSET(Binder2, p34,   0x34);
ASSERT_OFFSET(Binder2, p38,   0x38);

void BindLazyFive(Binder2* b)
{
    b->ready = 1;

    if ((g_lazy1Guard & 1) == 0)
    {
        g_lazy1Guard |= 1;
        g_lazy1.head = &g_lazy1Head;
    }
    b->p20 = &g_lazy1;

    if ((g_lazy2Guard & 1) == 0)
    {
        g_lazy2Guard |= 1;
        g_lazy2.head = &g_lazy2Head;
    }
    b->p1C = &g_lazy2;

    if ((g_lazy3Guard & 1) == 0)
    {
        g_lazy3Guard |= 1;
        g_lazy3.head = &g_lazy3Head;
    }
    b->p28 = &g_lazy3;

    if ((g_lazy6Guard & 1) == 0)
    {
        g_lazy6Guard |= 1;
        g_lazy6.head = &g_lazy6Head;
    }
    b->p2C = &g_lazy6;

    if ((g_lazy8Guard & 1) == 0)
    {
        g_lazy8Guard |= 1;
        g_lazy8.head = &g_lazy8Head;
    }
    b->p34 = &g_lazy8;

    if ((g_lazy8Guard & 1) == 0)
    {
        g_lazy8Guard |= 1;
        g_lazy8.head = &g_lazy8Head;
    }
    b->p38 = &g_lazy8;

    g_handlerSlot2.handler = (void*)&BoundHandler2;
}
