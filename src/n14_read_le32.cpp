// sub_82791438 -- read a little-endian 32-bit word out of a paged address
// space, one byte at a time. 128 B, 4 callers. /O2 /Os.
//
//      lwz    r11,24(r3)         base, +0x18
//      rlwinm r10,r4,3,0,28      n * 8
//      lwz    r9,16(r3)          the memory object, +0x10
//      add    r11,r10,r11
//      addi   r11,r11,4          a = base + n*8 + 4
//      addi   r10,r11,3          a+3
//      lwz    r7,12(r9)          the page table, +0x0C
//      addi   r9,r11,2           a+2
//      rlwinm r6,r10,22,10,29    (a+3) >> 12, scaled by 4
//      ...
//      lwzx   r6,r6,r7           pages[(a+3) >> 12]
//      lbzx   r10,r6,r10         that page[(a+3) & 0xFFF]
//      rotlwi r10,r10,8
//      or     r10,r10,r9
//      rlwinm r10,r10,8,0,23
//      or     r10,r10,r9'
//      rlwinm r10,r10,8,0,23
//      or     r3,r10,r11'
//
// THE PAGE SHIFT IS 12 AND THE SCALE IS 4, both read out of one instruction.
// `rlwinm rD,rS,22,10,29` rotates left 22 -- that is right by 10 -- and keeps
// LSB bits 2..21, which therefore hold source bits 12..31: the value is
// `(a >> 12) * 4`, an index of 20 bits into a table of 4-byte entries. The
// companion `clrlwi rD,rS,20` keeps 12 bits, so the page is 4096 bytes and
// the two halves account for the whole address with no overlap. Reading the
// ROTATE as the shift -- 10 -- would have invented a 1 KB page with a 4 KB
// offset, which is why the mask has to be decoded rather than eyeballed.
//
// THE ASSEMBLY IS LITTLE-ENDIAN and it is built by a left-leaning chain, not
// by four independent shifts: `rotlwi 8`, `or`, `<<8`, `or`, `<<8`, `or`. The
// byte at a+3 ends up in bits 24..31, so a 32-bit value is being reassembled
// from a byte order opposite to this machine's -- which is what a guest
// address space of another endianness looks like from a big-endian host.
// The four bytes are fetched HIGH FIRST, and that is the source's evaluation
// order, since each `lwzx` page lookup is issued just ahead of its own `lbzx`.
//
// TWO SEPARATE AXES, and neither is guessable from the other.
//
// (1) /O2 /Os. At plain /O2 this is 11 of 32 with the right instructions in
// nearly the right order and every register renamed, plus one transposed
// pair. The level takes it to 30 of 32.
//
// (2) IT IS A MEMBER FUNCTION. The last two words were `lwz r11,24(r3)` and
// `rlwinm r10,r4,3,0,28` -- the base and the scaled index arriving in each
// other's registers, with the `add r11,r10,r11` that consumes them
// BYTE-IDENTICAL either way. So the operand order of the `add` was never the
// question; what differs is which of the two values is defined first, and
// therefore which gets r11 from a descending allocation.
//
// Eleven spellings of the address leave it exactly where it was, all 30 of
// 32: naming the base in a local, naming the offset in a local, `n*8` written
// first, `+4` moved ahead of the multiply, the multiply and the 4
// parenthesised together, `n << 3` spelled out, `n * sizeof(Ent)`, a signed
// index, reading the page table before the address, and accumulating with
// `+=`. Making it a member of the context object is 32 of 32.
//
// That is MATCHED.md's member-function lever on a third symptom. It was
// recorded for transposed registers (sub_826C0FC8) and turned up again for
// load ISSUE order (src/n9_desc_equal.cpp); here it is which of two
// independent values is numbered first. `this` is not simply the first
// parameter as far as the front end is concerned, and no rearrangement of a
// free function's body reaches what it changes.
//
// Nothing is relocated; all 32 words are compared.

#include "types.h"

struct PagedMem
{
    /* 0x00 */ char unk0000[0x0C];
    /* 0x0C */ u8** pages;
};
ASSERT_OFFSET(PagedMem, pages, 0x0C);

static u8 ReadByte(u8** pages, u32 a)
{
    return pages[a >> 12][a & 0xFFF];
}

struct MemCtx
{
    /* 0x00 */ char      unk0000[0x10];
    /* 0x10 */ PagedMem* mem;
    /* 0x14 */ char      unk0014[0x04];
    /* 0x18 */ u32       base;

    u32 ReadEntryLe32(u32 n);
};
ASSERT_OFFSET(MemCtx, mem, 0x10);
ASSERT_OFFSET(MemCtx, base, 0x18);

u32 MemCtx::ReadEntryLe32(u32 n)
{
    u32  a = base + n * 8 + 4;
    u8** pages = mem->pages;

    return ((((ReadByte(pages, a + 3) << 8) | ReadByte(pages, a + 2)) << 8)
            | ReadByte(pages, a + 1)) << 8 | ReadByte(pages, a);
}
