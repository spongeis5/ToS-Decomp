"""Source shapes for sub_827FE808, for tools/permute.py.

    target:  lbz r11,106(r3) ; andi. r11,r11,243 ; stb r11,106(r3) ; blr
    ours:    lbz ; clrlwi r10,r11,24 ; rlwinm r10,r10,0,30,27 ; stb ; blr

`lbz` already zero-extends, so the `clrlwi` is redundant and the mask then
needs two instructions instead of one. The question is what makes the
compiler use the single `andi.` form.
"""

H = '#include "types.h"\n\n'

BODIES = [
    ("u8 field, &= 243", H + """
struct B106 { char unk0000[0x6A]; u8 flags; };
ASSERT_OFFSET(B106, flags, 0x6A);
void ClearBits23(B106* p) { p->flags &= 243; }
"""),
    ("u8 field, &= 0xF3", H + """
struct B106 { char unk0000[0x6A]; u8 flags; };
ASSERT_OFFSET(B106, flags, 0x6A);
void ClearBits23(B106* p) { p->flags &= 0xF3; }
"""),
    ("read into u8 local, mask, store", H + """
struct B106 { char unk0000[0x6A]; u8 flags; };
ASSERT_OFFSET(B106, flags, 0x6A);
void ClearBits23(B106* p) { u8 v = p->flags; p->flags = (u8)(v & 0xF3); }
"""),
    ("unsigned int temp", H + """
struct B106 { char unk0000[0x6A]; u8 flags; };
ASSERT_OFFSET(B106, flags, 0x6A);
void ClearBits23(B106* p)
{
    unsigned v = p->flags;
    p->flags = (u8)(v & 0xF3);
}
"""),
    ("explicit cast on the mask", H + """
struct B106 { char unk0000[0x6A]; u8 flags; };
ASSERT_OFFSET(B106, flags, 0x6A);
void ClearBits23(B106* p) { p->flags = p->flags & (u8)0xF3; }
"""),
    ("member function", H + """
struct B106
{
    char unk0000[0x6A];
    u8 flags;
    void Clear();
};
ASSERT_OFFSET(B106, flags, 0x6A);
void B106::Clear() { flags &= 0xF3; }
"""),
    ("char (signed) field", H + """
struct B106 { char unk0000[0x6A]; char flags; };
ASSERT_OFFSET(B106, flags, 0x6A);
void ClearBits23(B106* p) { p->flags &= 0xF3; }
"""),
    ("bitwise-and-not", H + """
struct B106 { char unk0000[0x6A]; u8 flags; };
ASSERT_OFFSET(B106, flags, 0x6A);
void ClearBits23(B106* p) { p->flags &= ~12; }
"""),
]
