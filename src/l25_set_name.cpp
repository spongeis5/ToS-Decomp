// sub_821FA140 -- record an owner and copy a name into a fixed global
// buffer. 24 B, 3 callers.
//
//      lis  r11,-32102
//      li   r5,128
//      addi r10,r11,31392        = 829A7AA0, the buffer
//      stw  r3,-132(r10)         = 829A7A1C, the owner word
//      mr   r3,r10
//      b    0x828A9968
//
// 828A9968 is `strncpy` from libcMT.lib by byte match (build/lib_matches.txt),
// so r3/r4/r5 are dst/src/count and the 128 is the buffer's size rather than
// a free constant.  The second parameter is passed straight through in r4
// and never touched, which is what says it is the source string.
//
// One relocated `lis`/`addi` pair and an unrelocated `-132` displacement off
// it: the owner word and the buffer are fields of ONE global object, 132
// bytes apart, and MSVC materialised the buffer's address because that one
// is passed as an argument.
//
// NOT MATCHED BY ONE WORD.  Six words, four relocated; of the two compared,
// one agrees.  The difference is the same one as sub_82158E50: ours
// materialises the record's own address in r11 and forms the argument with
// `addi r3,r11,132`, where the target materialises the BUFFER's address in
// r10 and reaches the owner word at `-132(r10)`, so its argument is a plain
// `mr r3,r10`.
//
// THE `-132` IS NOT A RELOCATION, and that is the whole of what is new here.
// A displacement the linker would fix up would be a relocated word; this one
// is a literal.  So retail's compiler KNEW the two objects were 132 bytes
// apart, which it only knows about statics in ONE translation unit -- and
// that is a different source model from the single record assumed above.
//
// TWO SAME-TU STATICS REPRODUCE THE TARGET'S SHAPE.  With
//
//      static char g_own[4];        /* or a u32, or a struct */
//      static char g_buf[128];
//
// MSVC emits exactly the target's six instructions at exactly 24 bytes --
//
//      lis r11 ; li r5,128 ; addi r10,r11,LOW(g_buf)
//      stw r3,<d>(r10) ; mr r3,r10 ; b strncpy
//
// -- with `mr r3,r10` byte-identical, `li r5,128` byte-identical, and three
// relocated.  2 of 3 compared words, against 1 of 2 for the record above.
// The single record can never produce that shape: MSVC will not fold +132
// into an external symbol's `addi` relocation, so it always spends a second
// `addi` and there is no `mr` to match.
//
// WHAT IS LEFT IS THE SIGN OF ONE DISPLACEMENT.  MSVC emits `+sizeof(the
// address-taken object)`, never a negative, so the store comes out
// `stw r3,128(r10)` where the image has `stw r3,-132(r10)`.  Twenty layouts
// were measured and the rule did not move once: declaration order both ways,
// sizes 4 / 128 / 132 / 256 / 260 (the displacement follows the buffer's size
// exactly -- a 260-byte buffer gives +260), `u32` against `char`, arrays
// against structs against an anonymous namespace, `__declspec(align(4))`,
// initialised against uninitialised, `#pragma bss_seg` splitting them, and
// extra functions in the translation unit taking the other symbol's address
// first.  Mixing `extern` with `static` costs a second `lis` (28 bytes).
//
// The remaining family -- one symbol AT the buffer with the owner reached by
// a negative cast, `((u32*)dst)[-33] = owner;` -- does produce `-132(r10)`
// and `mr r3,r10`, but MSVC schedules the argument copy ahead of the store
// and pays an `mr r11,r3` to free r3: 28 bytes, seven instructions, in all
// of nine spellings (bare cast, named local, `u32*` pin, a `NameRec` at the
// buffer, a `u32[32]` view, a container-of helper, a value local, a helper
// taking the pointer, a returned `char*`) and at /O2, /O2 /Os, /O1, /O2 /Ou
// and /Ox.
//
// The single-record reading is kept in the source because the 128 bytes
// between the two fields are unknown either way, and writing them as a
// fabricated neighbouring static claims more than the bytes show.  But the
// next attempt should start from the two-statics shape: it is five of six
// instructions exact, and what it needs is whatever makes MSVC place the
// stored-to object BEFORE the address-taken one.

#include "types.h"
#include <string.h>

struct NameBlock
{
    /* 0x00 */ u32  owner;
    /* 0x04 */ char unk0004[128];
    /* 0x84 */ char name[128];
};
ASSERT_OFFSET(NameBlock, name, 132);

extern NameBlock g_name_block;          /* 829A7A1C */

void SetOwnedName(u32 owner, const char* s)
{
    char* dst = g_name_block.name;

    g_name_block.owner = owner;
    strncpy(dst, s, sizeof(g_name_block.name));
}
