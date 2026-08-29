#include "types.h"

// sub_82790710 -- read a 1-to-4 byte variable-length integer out of a paged
// address space, store it through the third argument, return its length.
// 260 B, 5 callers. /O2 /Os.
//
// The paging is stated by two rotates:
//
//      rlwinm r11,r4,22,10,29     rotate right 10, keep bits 10..29
//                                 = ((a >> 12) & 0xFFFFF) * 4
//      clrlwi r8,r4,20            = a & 0xFFF
//
// so the page is 4 KB and the table is an array of pointers -- `(a>>12)*4`
// is a shift and a scale folded into one rotate, not a constant anyone wrote.
// `lwz r9,12(r10)` after `lwz r10,0(r3)` puts that table at +0x0C of the
// object held at +0x00 of the argument.
//
// THE FIRST BYTE CARRIES BOTH LENGTH AND PAYLOAD:
//
//      clrlwi  r8,r11,30          n = b0 & 3          -- 0..3, length-1
//      rlwinm  r11,r11,30,2,31    v = b0 >> 2         -- the low payload bits
//
// and v is computed BEFORE the dispatch and survives every branch in r11,
// which is why it is a local assigned up front rather than part of each arm.
//
// THE DISPATCH IS A SWITCH, not a chain of `==`. An if-chain over 0/1/2/3
// compares against 0, then 1, then 2; the image holds
// `cmplwi r8,1 / blt / beq / cmplwi r8,3 / blt`, MSVC's binary decision tree
// over the dense set {0,1,2,3} -- pivot at 1, remainder split at 3.
//
// Each longer arm prepends one more byte, big-endian. The 2-byte arm's shift
// is `rotlwi r10,r10,6` with no mask and the longer arms' is
// `rlwinm r10,r10,6,0,25` with one: MSVC knows a single byte cannot lose
// anything shifted left by 6, and will not assume it of two or three.
//
// ---------------------------------------------------------------------
// THREE THINGS DECIDED THIS ONE, and the third is a lever I have not seen
// written down.
//
// 1. THE STORE AND THE RETURN GO AFTER THE SWITCH, not inside its arms.
//    Written as `*out = ...; return N;` per case, MSVC gives every arm its
//    own `stw`/`ld r31`/`blr` -- 296 bytes against 260. The image has ONE
//    exit at 82790808 that all four arms reach with the value in r11, which
//    is a single variable assigned by the switch and stored once after it.
//
// 2. /Os, BY THE DOCUMENTED SIGNATURE. At /O2 the instructions and the
//    operand orders were already right and the DESTINATIONS were fresh
//    where retail reuses its source -- `lwzx r7,r11,r9` against
//    `lwzx r11,r11,r9`, then `lbzx r6,r7,r8` against `lbzx r11,r11,r8`,
//    the same value threaded through one register. That is MATCHED.md's
//    register-coalescing signature exactly. /Os also merged the duplicated
//    exits: 260 bytes, every register correct, 37 of 65.
//
// 3. THE SOURCE ORDER OF THE ARMS DECIDES WHICH ARM OWNS A SHARED TAIL,
//    even though it does NOT decide their layout order. The remaining 28
//    words were a pure permutation: the two-instruction tail `x <<= 6` /
//    `v |= x` is common to the 4-, 3- and 2-byte arms, and MSVC keeps one
//    copy. Written with the cases ascending, it kept the 3-byte arm's copy
//    and the 2-byte arm's copy, and the 4-byte arm branched FORWARD into
//    them. Retail keeps the 4-byte arm's copy and the shorter arms branch
//    BACKWARD into it (`b 8279079C`, `b 827907A0`).
//
//    Writing the same four arms in the opposite order -- 4-byte first, then
//    3, 2, 1 -- is 65 of 65 with nothing else changed. The arms' LAYOUT is
//    identical either way, because the decision tree emits its fall-through
//    case first and that is the 4-byte one regardless of source order; only
//    the ownership of the merged tail moved. So source order still carries
//    information about a switch after layout order has stopped carrying any.
//
//    MEASURED, all at /O2 /Os: ascending 37 of 65; descending 65 of 65; and
//    a third shape that spells the shared tail out structurally, with the
//    shorter arms entering it by `goto`, is 10 of 65 -- worse than either,
//    because forcing the join point also re-allocates every register in the
//    function.

struct PagedMem
{
    /* 0x00 */ char unk0000[0x0C];
    /* 0x0C */ u8** pages;
};
ASSERT_OFFSET(PagedMem, pages, 0x0C);

struct VarintCtx
{
    /* 0x00 */ PagedMem* mem;
};
ASSERT_OFFSET(VarintCtx, mem, 0x00);

static u8 Read8(PagedMem* m, u32 a)
{
    return m->pages[a >> 12][a & 0xFFF];
}

u32 ReadVarint(VarintCtx* c, u32 addr, u32* out)
{
    PagedMem* m = c->mem;

    u32 b0 = Read8(m, addr);
    u32 v  = b0 >> 2;
    u32 n;

    switch (b0 & 3)
    {
    default:
        v = (((((((u32)Read8(m, addr + 3)) << 8)
                | Read8(m, addr + 2)) << 8)
              | Read8(m, addr + 1)) << 6) | v;
        n = 4;
        break;

    case 2:
        v = (((((u32)Read8(m, addr + 2)) << 8)
              | Read8(m, addr + 1)) << 6) | v;
        n = 3;
        break;

    case 1:
        v = ((u32)Read8(m, addr + 1) << 6) | v;
        n = 2;
        break;

    case 0:
        n = 1;
        break;
    }

    *out = v;
    return n;
}
