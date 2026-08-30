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
// fabricated neighbouring static claims more than the bytes show.
//
// ---- WHAT DECIDES THE ANCHOR, measured this session ----
//
// The advice this comment used to end with -- "start from the two-statics
// shape and find whatever makes MSVC place the stored-to object BEFORE the
// address-taken one" -- is answered, and the answer is that the LAYOUT was
// never the obstacle. Three measurements, in the order they were made:
//
// **(1) MSVC anchors on the LOWEST-ADDRESSED object the function
// references.** Not on the address-taken one, and not on the one at .bss+0:
// a probe whose second function touches only a higher static emits a COFF
// symbol for it at .bss+0x84 and anchors there, so a non-zero-offset anchor
// is perfectly possible -- it just never happens when something lower is
// also referenced.
//
// **(2) MSVC will NOT fold a constant byte offset into a symbol's own `addi`
// relocation.** Eleven spellings: a `static` record and an `extern` one, a
// member, `&r.name[0]`, a one-element array of the record, a `char[260]`
// plus 132, a `u32[65]` plus 33, the address in a named local declared
// before or after the store, and an anonymous namespace. Every one
// materialises the record base and spends a SECOND `addi` -- which is the
// `addi r3,r11,132` this file emits, and the one word it is short.
//
// Together (1) and (2) say the two shapes are mutually exclusive: the
// `mr r3,r10` needs the anchor to BE the buffer, and a negative store
// displacement needs the anchor to be ABOVE the stored word. Both were
// checked directly rather than reasoned about --
//
//      owner 132 bytes ABOVE the buffer   lis, li, addi, stw +132, mr, b
//                                         24 B, the target's shape, wrong sign
//      owner 132 bytes BELOW the buffer   lis, mr, addi, li, addi, stw 0, b
//                                         28 B, anchor moves to the owner
//      one symbol AT the buffer, owner
//        reached at [-33]                 lis, mr, addi, li, mr, stw -132, b
//                                         28 B, right displacement, two `mr`s
//
// -- the last row being twelve further spellings (bare cast, `*(u32*)(buf -
// 132)`, a named `char* d`, a named `u32* p`, a struct at the buffer with a
// negative member, an inlined helper taking the buffer, an `extern` and a
// `static` buffer, the owner as the second parameter, a `volatile` cast, the
// `strncpy` result forwarded, and `sizeof(g_buf)` for the count) at /O2 and
// at /O2 /Os. All twelve are byte-identical.
//
// **(3) The .bss order of uninitialised statics is not declaration order,
// size order or alignment order -- it moves with the NAME, and not
// alphabetically.** Measured by folding `(char*)&A - (char*)&B` to a
// constant and reading it off the `addi`: `g_head`(132) with `g_buf`(128)
// puts the buffer first, while `g_ahead` and `g_zhead` -- the same two
// objects, one character different in the name -- put the head first. Also
// measured: a `char[N]` for N >= 8 takes 8-byte alignment, so a 132-byte
// object is followed at +136 and not +132. None of this matters given (1),
// and it is recorded so the next reader does not spend the afternoon on it.
//
// **The companion function came out; this one did not, and the difference is
// the STORE.** sub_82158E50 is the same "one global record, negative
// displacements" shape and it now matches, because all three of its accesses
// are LOADS: reaching them at `[-8]` and `[-7]` off an `extern` at the
// anchor costs nothing. Here the value being stored arrives in r3 and r3 is
// also the outgoing argument, so any spelling that derives the store's base
// from the pointer being passed makes MSVC stage the argument copy first and
// pay to save the owner. What is still not tried is anything that separates
// those two roles of r3.

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
