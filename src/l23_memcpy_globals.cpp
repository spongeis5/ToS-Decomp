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
// NOT MATCHED, and the residue is WHICH FIELD BECOMES THE BASE.  Of the six
// words four are relocated; of the two that are compared, both differ, and
// they differ only in that ours materialises the record's own address and
// reads +4 and +32 from it while the target materialises the src field's
// address and reads -28 and -32.  The emitted load order follows: ours is
// dst, size, src (ascending), the target's is src, size, dst (descending),
// and the first load is the one whose address becomes the base in both.
//
// Naming all three in locals in the target's order changes NOTHING -- the
// generated code is identical to the direct spelling, so the load order is
// not source-readable here the way declaration order was for sub_82691B70.
// The remaining ideas all require the base symbol to sit at the src field,
// which no C declaration of one record expresses.

#include "types.h"
#include <string.h>

struct CopyRequest
{
    /* 0x00 */ void* dst;
    /* 0x04 */ u32   size;
    /* 0x08 */ char  unk0008[0x18];
    /* 0x20 */ void* src;
};
ASSERT_OFFSET(CopyRequest, size, 0x04);
ASSERT_OFFSET(CopyRequest, src,  0x20);

extern CopyRequest g_copy_request;      /* 8299B854 */

void RunPendingCopy()
{
    void* src = g_copy_request.src;
    u32   n   = g_copy_request.size;
    void* dst = g_copy_request.dst;

    memcpy(dst, src, n);
}
