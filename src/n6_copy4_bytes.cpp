// sub_8272CB78 -- copy four bytes, one field at a time. 36 B, 4 callers.
//
//      lbz     r11,0(r4) ; stb r11,0(r3)
//      lbz     r10,1(r4) ; stb r10,1(r3)
//      lbz     r9,2(r4)  ; stb r9,2(r3)
//      lbz     r8,3(r4)  ; stb r8,3(r3)
//      blr
//
// EVERY LOAD STAYS BEHIND THE PREVIOUS STORE, which is the whole content of
// this function: MSVC cannot prove that `d` and `s` do not overlap, so the
// pairs are pinned in source order. Compare src/n5_set_bit_pair.cpp, where
// two offsets off ONE base are provably distinct and both loads are hoisted
// above both stores. So the two pointers are separate parameters, not two
// views of one object.
//
// Four byte moves rather than one `lwz`/`stw`: MSVC does not merge adjacent
// byte stores at /O2, so this is four written-out field assignments and not
// an implicit 4-byte struct copy, which would have used a word.
//
// Nothing here decides whether the function returns `void` or returns its
// destination -- r3 is never modified, so `Quad4& operator=` would emit the
// same eight instructions. The weaker claim is the one written.
//
// Its immediate predecessor sub_8272CB68 (src/load_global_store.cpp, 16 B,
// ends exactly at 8272CB78) is matched at plain /O2, and adjacency is the
// evidence for the level -- see MATCHED.md, "Flags are a property of the
// translation unit".
//
// Nothing is relocated; all 9 words are compared.

#include "types.h"

struct Quad4
{
    /* 0x00 */ u8 a;
    /* 0x01 */ u8 b;
    /* 0x02 */ u8 c;
    /* 0x03 */ u8 d;
};
ASSERT_OFFSET(Quad4, d, 0x03);

void CopyQuad4(Quad4* dst, const Quad4* src)
{
    dst->a = src->a;
    dst->b = src->b;
    dst->c = src->c;
    dst->d = src->d;
}
