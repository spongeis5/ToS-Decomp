// sub_82790B18 -- read one byte through a two-level page table, look its low
// nibble up in a 16-entry length table, and copy that many more bytes.
// 116 bytes, 4 callers.
//
//      lwz    r11,0(r3) ; lwz r11,12(r11)
//      rlwinm r8,r4,22,10,29        (a >> 12) * 4
//      clrlwi r7,r4,20              a & 0xFFF
//      lwzx   r11,r8,r11 ; lbzx r11,r11,r7
//      stb    r11,0(r5)
//      clrlwi r11,r11,24 ; clrlwi r11,r11,28
//      lbzx   r11,r11,r6            g_runLen[c & 15], base 82086598
//      cmplwi r11,0 ; beq- <done> ; mtctr r11
//      addi   r9,r9,-1
//  loop:  (the whole page walk again, inlined a second time)
//      lwz r8,0(r3) ; lwz r8,12(r8) ; lwzx r8,r7,r8 ; lbzx r8,r8,r6
//      stbu r8,1(r9) ; bdnz+
//  done:
//      addi r3,r11,1 ; blr
//
// `rlwinm rX,rA,22,10,29` is one instruction doing `(a >> 12) * 4`: rotating
// right by 10 and keeping bits 10..29 drops the low twelve bits and leaves
// the result already scaled for a word index. Its partner `clrlwi 20` keeps
// exactly those twelve, so the page is 4096 bytes and the directory is an
// array of pointers.
//
// The walk appears TWICE with the base pointers reloaded, which is an inlined
// accessor called once before the loop and once inside it, not a hoisted
// local -- a local would have been kept in a register.
//
// The doubled mask `clrlwi 24` then `clrlwi 28` is the u8 return being
// normalised and then masked to a nibble; the `stb` needs neither, so the
// first one exists only because the value passed through a u8.
//
// The table at 82086598 is 1,2,1,2,1,2,3,4,2,3,4,5,6,7,8,9 -- sixteen
// entries, read out of the image. It holds no zero, so the `beq-` skip is
// unreachable in practice and is the compiler's, not the source's.
//
// `mtctr` + `bdnz` is a counted loop, so the trip count is computed once.
//
// 1 of 29 words is relocated.

#include "types.h"

struct PageDir
{
    /* 0x00 */ u8  unk0000[12];
    /* 0x0C */ u8** pages;
};

ASSERT_OFFSET(PageDir, pages, 0x0C);

struct MemCtx
{
    /* 0x00 */ PageDir* dir;
};

extern const u8 g_runLen[16];

static u8 ReadByte(MemCtx* c, u32 a)
{
    return c->dir->pages[a >> 12][a & 0xFFF];
}

u32 ReadRun(MemCtx* c, u32 addr, u8* out)
{
    u8 b = ReadByte(c, addr);
    out[0] = b;

    u32 n = g_runLen[b & 15];

    for (u32 i = 0; i < n; i++)
        out[i + 1] = ReadByte(c, addr + 1 + i);

    return n + 1;
}
