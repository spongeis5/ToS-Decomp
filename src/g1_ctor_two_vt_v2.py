"""Round 2 for sub_82784F90 -- one four-word scheduling window.

Round 1 was 25 of 29 non-relocated words at /O2 /Os with the size exactly
right.  The whole remainder is that the second vtable's high half is
materialised ONE SLOT too early, which pushes the +20 store behind the +24
store:

    want  li r10,1 ; lis r9,vtB@ha ; li r11,0 ; stw r10,20 ; addi r10,r9 ; stw r11,24
    got   lis r10,vtB@ha ; li r11,0 ; li r9,1 ; addi r10,r10 ; stw r11,24 ; stw r9,20

Five placements of the vtable store were measured in round 1 -- after f37
(25), between the Zero4 and f36 (22), after f40/f44 (22), before the second
Zero4 (16) and last (16) -- so its position in the source is settled and the
remaining question is what else is in flight.  These vary the float block:
helper against inline, on either half, and the order of the +20 store against
the three zeroes.
"""

H = '#include "types.h"\n#include <ppcintrinsics.h>\n\n'

DECL = """
struct VTa;
struct VTb;
extern const VTa kVT_82085DD8;
extern const VTb kVT_82085E98;

struct Quad
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};

static void Zero4(Quad* q)
{
    q->x = 0.0f;
    q->y = 0.0f;
    q->z = 0.0f;
    q->w = 0.0f;
}

struct Item
{
    /* 0x00 */ const VTa* vt;
    /* 0x04 */ Quad q;
    /* 0x14 */ s32  f20;
    /* 0x18 */ s32  f24;
    /* 0x1C */ s32  f28;
    /* 0x20 */ s32  f32;
    /* 0x24 */ u8   f36;
    /* 0x25 */ u8   f37;
    /* 0x26 */ char unk0026[2];
    /* 0x28 */ s32  f40;
    /* 0x2C */ s32  f44;
    /* 0x30 */ u16  f48;
    /* 0x32 */ u16  f50;
    /* 0x34 */ s32  f52;
    /* 0x38 */ s32  f56;
    /* 0x3C */ s32  f60;
    /* 0x40 */ u16  f64;
};
ASSERT_OFFSET(Item, q, 4);
ASSERT_OFFSET(Item, f36, 36);
ASSERT_OFFSET(Item, f50, 50);
ASSERT_OFFSET(Item, f64, 64);
"""

INLINE4 = ("    it->q.x = 0.0f;\n    it->q.y = 0.0f;\n"
           "    it->q.z = 0.0f;\n    it->q.w = 0.0f;\n")
CALL4 = "    Zero4(&it->q);\n"

REST = """    it->f40 = 0;
    it->f44 = 0;
    it->f48 = 0;
    it->f50 = kind;
    it->f52 = 0;
    it->f56 = 0;
    it->f60 = 0;
    it->f64 = 0;
"""

VT_B = "    it->vt = (const VTa*)&kVT_82085E98;\n"
B2 = "    it->f36 = 0;\n    it->f37 = 0;\n"
ZEROS = "    it->f24 = 0;\n    it->f28 = 0;\n    it->f32 = 0;\n"
ONE = "    it->f20 = 1;\n"


def mk(first, second, mid):
    return (H + DECL +
            "\nvoid ConstructItem(Item* it, u16 kind)\n{\n"
            "    it->vt = (const VTa*)&kVT_82085DD8;\n" + first +
            "    __lwsync();\n" + mid + second + B2 + VT_B + REST + "}\n")


BODIES = [
    ("helper both, 20 then 24/28/32", mk(CALL4, CALL4, ONE + ZEROS)),
    ("helper both, 24/28/32 then 20", mk(CALL4, CALL4, ZEROS + ONE)),
    ("helper base, inline derived", mk(CALL4, INLINE4, ONE + ZEROS)),
    ("inline base, helper derived", mk(INLINE4, CALL4, ONE + ZEROS)),
    ("inline both", mk(INLINE4, INLINE4, ONE + ZEROS)),
    ("helper both, second Zero4 before the zeroes",
     H + DECL + """
void ConstructItem(Item* it, u16 kind)
{
    it->vt = (const VTa*)&kVT_82085DD8;
    Zero4(&it->q);
    __lwsync();
    it->f20 = 1;
    Zero4(&it->q);
    it->f24 = 0;
    it->f28 = 0;
    it->f32 = 0;
    it->f36 = 0;
    it->f37 = 0;
    it->vt = (const VTa*)&kVT_82085E98;
""" + REST + "}\n"),
    ("vtB through a named local", H + DECL + """
void ConstructItem(Item* it, u16 kind)
{
    it->vt = (const VTa*)&kVT_82085DD8;
    Zero4(&it->q);
    __lwsync();
    it->f20 = 1;
    it->f24 = 0;
    it->f28 = 0;
    it->f32 = 0;
    Zero4(&it->q);
    it->f36 = 0;
    it->f37 = 0;
    const VTa* v = (const VTa*)&kVT_82085E98;
    it->vt = v;
""" + REST + "}\n"),
    ("Zero4 taking Item*", H + """
struct VTa;
struct VTb;
extern const VTa kVT_82085DD8;
extern const VTb kVT_82085E98;

struct Quad { f32 x; f32 y; f32 z; f32 w; };

struct Item
{
    /* 0x00 */ const VTa* vt;
    /* 0x04 */ Quad q;
    /* 0x14 */ s32  f20;
    /* 0x18 */ s32  f24;
    /* 0x1C */ s32  f28;
    /* 0x20 */ s32  f32;
    /* 0x24 */ u8   f36;
    /* 0x25 */ u8   f37;
    /* 0x26 */ char unk0026[2];
    /* 0x28 */ s32  f40;
    /* 0x2C */ s32  f44;
    /* 0x30 */ u16  f48;
    /* 0x32 */ u16  f50;
    /* 0x34 */ s32  f52;
    /* 0x38 */ s32  f56;
    /* 0x3C */ s32  f60;
    /* 0x40 */ u16  f64;
};
ASSERT_OFFSET(Item, f64, 64);

static void ZeroQuad(Item* it)
{
    it->q.x = 0.0f;
    it->q.y = 0.0f;
    it->q.z = 0.0f;
    it->q.w = 0.0f;
}

void ConstructItem(Item* it, u16 kind)
{
    it->vt = (const VTa*)&kVT_82085DD8;
    ZeroQuad(it);
    __lwsync();
    it->f20 = 1;
    it->f24 = 0;
    it->f28 = 0;
    it->f32 = 0;
    ZeroQuad(it);
    it->f36 = 0;
    it->f37 = 0;
    it->vt = (const VTa*)&kVT_82085E98;
""" + REST + "}\n"),
]
