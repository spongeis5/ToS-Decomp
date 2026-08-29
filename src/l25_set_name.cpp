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
// Naming the buffer in a local first does not move it -- the code is
// identical either way.  What would produce the target is a base symbol AT
// the buffer with the owner word before it, which one C record cannot
// express; reaching it through a cast off the buffer pointer would, and is
// not written here because an ugly spelling that happens to match is a
// claim about the source, not a reading of it.

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
