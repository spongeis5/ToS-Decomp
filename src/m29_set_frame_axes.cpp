// sub_822152C0 -- fill three 12-byte vectors of a frame reached through a
// field. 96 bytes, 4 callers.
//
//      lwz  r11,56(r3)        the frame
//      lwz  r9,0(r4) ; stw r9,32(r11)      \
//      lwz  r6,4(r4) ; stw r6,36(r11)       |  a 12-byte STRUCT COPY:
//      lwz  r5,8(r4) ; stw r5,40(r11)      /   lwz/stw, not lfs/stfs
//      lwz  r3,11424(r10) ; stw r3,16(r11) \
//      lwz  r10,4(r7)     ; stw r10,20(r11) |  the same, from a global
//      lwz  r9,8(r7)      ; stw r9,24(r11) /
//      lfs  f13,8(r4)  ; stfs f13,0(r11)
//      stfs f0,4(r11)                          f0 was loaded from 82002DA4
//      lfs  f12,0(r4)  ; fneg f11,f12 ; stfs f11,8(r11)
//
// Two kinds of copy in one function, and the difference is the source. A
// whole-vector ASSIGNMENT goes through integer registers -- MSVC copies a
// small POD by words and never looks at the member types -- while assigning
// one float member emits lfs/stfs. So the two word-triples are `= *v` and
// `= g_up`, and the last three stores are written out component by component.
//
// The constants, read out of the image rather than guessed:
//      82002CA0  0.0f 1.0f 0.0f   -- the global vector, a unit +Y
//      82002DA4  0.0f             -- the literal stored at +4
//
// The global's first component folds its low half into the `lwz`; components
// 1 and 2 need the address in a register first, because MSVC will not combine
// a relocated immediate with a constant. Same split as src/global_field.cpp.
//
// Store order is source order: +32.. then +16.. then +0, +4, +8.
//
// 4 of 24 words are relocated.

#include "types.h"

struct Vec3
{
    f32 x;
    f32 y;
    f32 z;
};

ASSERT_SIZE(Vec3, 12);

struct Frame
{
    /* 0x00 */ Vec3 axis;
    /* 0x0C */ u8   unk000C[4];
    /* 0x10 */ Vec3 up;
    /* 0x1C */ u8   unk001C[4];
    /* 0x20 */ Vec3 dir;
};

ASSERT_OFFSET(Frame, up, 0x10);
ASSERT_OFFSET(Frame, dir, 0x20);

struct FrameOwner
{
    /* 0x00 */ u8     unk0000[0x38];
    /* 0x38 */ Frame* frame;
};

ASSERT_OFFSET(FrameOwner, frame, 0x38);

extern const Vec3 g_worldUp;

void SetFrameAxes(FrameOwner* o, const Vec3* dir)
{
    Frame* f = o->frame;

    f->dir = *dir;
    f->up = g_worldUp;
    f->axis.x = dir->z;
    f->axis.y = 0.0f;
    f->axis.z = -dir->x;
}
