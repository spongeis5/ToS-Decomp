// sub_822306D8 -- set a ready flag, publish six lazily-constructed singleton
// objects into six fields, and install one handler. 344 bytes, no direct
// callers (reached through a vtable).
//
// The six blocks are byte-identical in shape and are MSVC's FUNCTION-LOCAL
// STATIC guard, one guard word per object:
//
//      lis   r11,-32101 ; addi r11,r11,-15584     &obj      = 829AC320
//      lwz   r10,-15580(r9)                        guard     = 829AC324
//      clrlwi r8,r10,31 ; cmplwi cr6,r8,0 ; bne-  if ((guard & 1) == 0)
//      lis   r8,-32255  ; ori r10,r10,1
//      addi  r8,r8,-17772                          the value stored at +0
//      stw   r10,-15580(r9)                        guard |= 1   FIRST
//      stw   r8,0(r11)                             then construct
//      stw   r11,32(r3)                            publish &obj
//
// The guard is set BEFORE the construction, which is MSVC's order, and no
// `atexit` registration follows any of them, so none of these objects has a
// destructor to run.
//
// The guard and the object are SEPARATE symbols that the linker happened to
// place adjacently: the guard is read with one `lis` and a displacement --
// a global scalar -- while the object's address is built with `lis`/`addi`
// because it is also the value stored into the field. Each block
// re-materialises its own `lis`; nothing is shared between them.
//
//      829AC320 / 829AC324  ->  +32
//      829AC328 / 829AC32C  ->  +28
//      829AC330 / 829AC334  ->  +40
//      829AC338 / 829AC33C  ->  +36
//      829AC340 / 829AC344  ->  +48
//      829AC348 / 829AC34C  ->  +44
//
// The field order 32, 28, 40, 36, 48, 44 is not address order, and store
// order is source order, so that is the order the six assignments are
// written in.
//
// `li r10,1` serves the `stb r10,24(r3)` and is then reused for the first
// guard, which is why the byte store is issued before any of them.

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

extern LazySingleton g_lazy4;       /* 829AC338 */
extern u32           g_lazy4Guard;  /* 829AC33C */
extern const void*   g_lazy4Head;   /* 8200BAB8 */

extern LazySingleton g_lazy5;       /* 829AC340 */
extern u32           g_lazy5Guard;  /* 829AC344 */
extern const void*   g_lazy5Head;   /* 8200BABC */

extern LazySingleton g_lazy6;       /* 829AC348 */
extern u32           g_lazy6Guard;  /* 829AC34C */
extern const void*   g_lazy6Head;   /* 8200BAC0 */

struct HandlerSlot
{
    /* 0x00 */ void* unk0000;
    /* 0x04 */ void* handler;
};
ASSERT_OFFSET(HandlerSlot, handler, 0x04);

extern HandlerSlot g_handlerSlot;   /* 82955B18 */

void BoundHandler();                /* 82200E00 */

struct Binder
{
    /* 0x00 */ u8             unk0000[0x18];
    /* 0x18 */ u8             ready;
    /* 0x19 */ u8             unk0019[0x03];
    /* 0x1C */ LazySingleton* p1C;
    /* 0x20 */ LazySingleton* p20;
    /* 0x24 */ LazySingleton* p24;
    /* 0x28 */ LazySingleton* p28;
    /* 0x2C */ LazySingleton* p2C;
    /* 0x30 */ LazySingleton* p30;
};
ASSERT_OFFSET(Binder, ready, 0x18);
ASSERT_OFFSET(Binder, p1C,   0x1C);
ASSERT_OFFSET(Binder, p20,   0x20);
ASSERT_OFFSET(Binder, p24,   0x24);
ASSERT_OFFSET(Binder, p28,   0x28);
ASSERT_OFFSET(Binder, p2C,   0x2C);
ASSERT_OFFSET(Binder, p30,   0x30);

void BindLazySix(Binder* b)
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

    if ((g_lazy4Guard & 1) == 0)
    {
        g_lazy4Guard |= 1;
        g_lazy4.head = &g_lazy4Head;
    }
    b->p24 = &g_lazy4;

    if ((g_lazy5Guard & 1) == 0)
    {
        g_lazy5Guard |= 1;
        g_lazy5.head = &g_lazy5Head;
    }
    b->p30 = &g_lazy5;

    if ((g_lazy6Guard & 1) == 0)
    {
        g_lazy6Guard |= 1;
        g_lazy6.head = &g_lazy6Head;
    }
    b->p2C = &g_lazy6;

    g_handlerSlot.handler = (void*)&BoundHandler;
}
