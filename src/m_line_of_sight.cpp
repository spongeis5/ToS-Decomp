#include "types.h"
#include <ppcintrinsics.h>
#include <vectorintrinsics.h>

// sub_82216918 -- TtCheckLineOfSight. 304 B.
//
// THE FIRST FUNCTION IN THIS PROJECT WITH ITS REAL NAME. It is not invented:
// the function pushes the string at 8200BA04 into the profiler, and that
// string is "TtCheckLineOfSight". `tools/profnames.py` recovers 100+ of
// these, so every one of them can be named truthfully rather than described.
//
// It is the AI visibility test -- can this thing see that thing -- and it
// works by casting a ray between two points and reporting whether anything
// was hit.
//
//      mflr r12 ; bl 0x828A75C4 (__savegprlr_27) ; stwu r1,-208(r1)
//      lwz  r31,0(r13)         the thread block
//      li   r30,48
//      lwzx r10,r30,r31        t_profiler -- the TLS READ form
//      lwz  r3,12(r10)         end
//      lwz  r9,4(r10)          cursor
//      cmplw cr6,r9,r3 ; bge- skip
//      stw  r6,0(r9)           <- "TtCheckLineOfSight"
//      mftb r5
//      stw  r5,4(r9)
//      addi r7,r9,12           entries are 12 bytes
//      stw  r7,4(r10)
// skip:...the work...
//      ...the same six instructions again with "Et" at 820074E4...
//      lbz  r10,144(r1)        the collector's hit flag
//      cntlzw r9,r10
//      rlwinm r3,r9,27,31,31   return !hit
//
// THE PROFILER SCOPE IS INLINED, and recognising it is worth more than this
// one function: the push and the pop are the same six instructions with a
// different string, and every `Tt*` function in the image begins and ends
// with them. The `lwz rX,0(r13)` + bare `li 48` + `lwzx` is the documented
// __declspec(thread) READ form.
//
// The two points are built as 16-BYTE vectors through one shared stack slot:
// three floats and a zero go to [80], `lvx128` lifts the lot into a vector
// register, the slot is rewritten with the other point, and the two vectors
// are stored out to [96] and [112]. That is MSVC copying two aligned 16-byte
// structs, not vector maths -- there is no arithmetic anywhere in it.
//
// The collector at [128] is a Havok-style ray-cast result: vtable at +0,
// an early-out fraction of 1.0f at +4, two constructor arguments at +8 and
// +12, and the hit flag at +16 that the return value reads.
//
// THREE THINGS HAD TO BE RIGHT, and each was a separate wrong answer:
//
//  1. `__vector4`, not an aligned struct. A struct of four floats aligned to
//     16 copies with `ld`/`std` -- two 64-bit integer moves -- and no vector
//     register ever appears. Only a real vector type emits lvx128/stvx128.
//     That was 24 of 76 against 60 of 76.
//  2. ONE scratch, assigned twice. Writing two separate Vec4s directly gives
//     twelve `stfs` and no copy at all. The target rebuilds the same slot at
//     [80] for both points.
//  3. A HELPER RETURNING THE VECTOR BY VALUE. With the scratch and the two
//     copies written inline, the two `stvx128` stores come out in the
//     opposite order and nothing moves them -- not declaring the two results
//     up front, in either order, and not /Os, which is 19 of 76. Written as
//     `Vec4 to4 = MakePoint(target); Vec4 from4 = MakePoint(self);` the
//     order is right and it is 62 of 62. The return-by-value is what ties
//     each store to its own copy instead of leaving both free to schedule.
struct ProfileEntry
{
    const char* name;
    u32         stamp;
    u32         unk0008;
};
ASSERT_SIZE(ProfileEntry, 12);

struct ProfileBuffer
{
    char          unk0000[4];
    ProfileEntry* cursor;
    char          unk0008[4];
    ProfileEntry* end;
};
ASSERT_OFFSET(ProfileBuffer, cursor, 4);
ASSERT_OFFSET(ProfileBuffer, end, 12);

__declspec(thread) ProfileBuffer* t_profiler;

static void ProfileMark(const char* name)
{
    ProfileBuffer* p = t_profiler;
    ProfileEntry* e = p->cursor;
    if (e < p->end)
    {
        e->name = name;
        e->stamp = (u32)__mftb32();
        p->cursor = e + 1;
    }
}

struct Vec3
{
    f32 x;
    f32 y;
    f32 z;
};

// A __vector4, not an aligned struct of four floats. Aligning a plain
// struct to 16 gets the copy done with `ld`/`std` -- two 64-bit integer
// moves -- and the vector registers never appear. Only a genuine vector
// type produces the lvx128/stvx128 pair the target uses.
typedef __vector4 Vec4;
ASSERT_SIZE(Vec4, 16);

struct __declspec(align(16)) Vec4Build
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};
ASSERT_SIZE(Vec4Build, 16);

struct CollectorVT;
extern const CollectorVT kRayCollectorVT_8200B08C;

struct RayCollector
{
    /* 0x00 */ const CollectorVT* vt;
    /* 0x04 */ f32   earlyOut;
    /* 0x08 */ void* filterA;
    /* 0x0C */ void* filterB;
    /* 0x10 */ u8    hit;
};
ASSERT_OFFSET(RayCollector, filterA, 0x08);
ASSERT_OFFSET(RayCollector, filterB, 0x0C);
ASSERT_OFFSET(RayCollector, hit, 0x10);

static Vec4 MakePoint(const Vec3* p)
{
    Vec4Build t;
    t.x = p->x;
    t.y = p->y;
    t.z = p->z;
    t.w = 0.0f;
    return *(const Vec4*)&t;
}

void CastRay(const Vec4* from, const Vec4* to, RayCollector* out,
             void* world, int flags);

int TtCheckLineOfSight(const Vec3* self, const Vec3* target,
                       void* filterA, void* filterB, void* world)
{
    RayCollector out;
    out.vt = &kRayCollectorVT_8200B08C;
    out.filterA = filterA;
    out.filterB = filterB;
    out.earlyOut = 1.0f;
    out.hit = 0;

    ProfileMark("TtCheckLineOfSight");

    // ONE scratch, assigned twice and copied out twice. The target rebuilds
    // the SAME stack slot at [80] for both points and vector-copies it to
    // [96] and [112]; two separate Vec4s written directly would be twelve
    // stfs and no vector registers at all.
    Vec4 to4 = MakePoint(target);
    Vec4 from4 = MakePoint(self);

    CastRay(&from4, &to4, &out, world, 0);

    ProfileMark("Et");

    return out.hit == 0;
}
