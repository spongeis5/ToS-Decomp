// sub_82158E50 -- run a copy whose three arguments all live in one global
// record. 24 B, 3 callers.
//
//      lis  r11,-32102
//      addi r10,r11,-18316       = 8299B874
//      lwz  r4,-18316(r11)       [8299B874]  -> src
//      lwz  r5,-28(r10)          [8299B858]  -> size
//      lwz  r3,-32(r10)          [8299B854]  -> dst
//      b    0x828A8CF0
//
// The callee is named by byte match, not guessed: build/lib_matches.txt has
// 828A8CF0 as `memcpy` from libcMT.lib's memcpyp.obj, so the argument roles
// are fixed and with them the field roles.
//
// THE NEGATIVE DISPLACEMENTS PROVE ONE OBJECT.  Only the `lis`/`addi` pair
// carries a relocation; `-28(r10)` and `-32(r10)` do not, so the compiler
// knew those two addresses relative to a symbol at link time -- which it can
// only do inside ONE symbol.  Three separate externs would each need their
// own relocated pair.  So this is one global record with dst at +0, size at
// +4 and src at +32, and the base MSVC chose to materialise is the src field.
//
// MATCHED at /O2, and what was wrong before was the ANCHOR, which is
// selectable from the source once two measurements are in hand.
//
// (1) THE EMITTED LOAD ORDER IS "ANCHOR FIRST, THEN THE REMAINING ARGUMENT
//     REGISTERS IN DESCENDING ORDER."  Measured on four layouts:
//
//       record dst+0 size+4 src+32   anchor dst(r3) -> r3, r5, r4
//       three statics, size lowest   anchor size(r5) -> r5, r4, r3
//       record src+0 dst+32 size+36  anchor src(r4) -> r4, r5, r3
//       the image                    anchor src(r4) -> r4, r5, r3
//
//     So the image's order is not a scheduling accident to be steered with
//     locals -- naming the three reads in the target's order really does
//     change nothing, as this file used to record. It follows from which
//     field is the anchor, and nothing else.
//
// (2) MSVC ANCHORS ON THE LOWEST-ADDRESSED OBJECT THE FUNCTION REFERENCES,
//     and it will NOT fold a member offset into a symbol's own `addi`
//     relocation -- checked eleven ways (static and extern record, a member,
//     `&r.name[0]`, a one-element array, a char array plus a byte offset, a
//     u32 array plus an index, with and without a named local). Every one
//     materialises the record base and spends a SECOND `addi`.
//
// Together those say the record's base symbol cannot be the dst end: a
// struct has no negative member offsets, and if the record's symbol were at
// dst then dst would be the anchor. The symbol the compiler had is at the
// SRC field, and the other two fields are reached BELOW it.
//
// Reaching them at the ACCESS is what keeps it to six instructions. Forming
// a re-based `CopyRequest* r = (CopyRequest*)((char*)&g_copy_src - 0x20);`
// and reading `r->dst` / `r->size` costs an extra `addi r10,r11,-32` (28
// bytes), because MSVC then has two live addresses instead of one.
//
// WHAT THE BYTES DO NOT SAY: four of the six words are relocated, so the
// spelling is under-determined -- naming `base` in a local and writing the
// two casts out at each use are byte-identical, as is a single `u32*` view
// used for all three fields. What is pinned is the two compared words, the
// displacements -28 and -32, and the anchor.

#include "types.h"
#include <string.h>

/* The record's SRC field, at 8299B874, is the symbol the compiler had.
 * dst is 32 bytes below it and size 28 below, in the same object:
 *
 *      8299B854   void* dst
 *      8299B858   u32   size
 *      8299B85C   (0x18 bytes not known)
 *      8299B874   void* src      <- g_copy_src
 */
extern void* g_copy_src;

void RunPendingCopy()
{
    void** base = (void**)&g_copy_src;

    memcpy(base[-8], g_copy_src, ((const u32*)base)[-7]);
}
