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
//
// NEAR MISS, and the mechanism is INDUCTION-VARIABLE ELIMINATION, not a
// statement order. Retail carries THREE inductions through the loop -- the
// ctr, the guest address in r10, and the output pointer in r9 using the
// update-form `stbu r8,1(r9)`. Every spelling tried instead lets MSVC notice
// that `out + 1 + i` and `addr + 1 + i` differ by a constant, hoist
// `subf r8,r4,r5` out of the loop and drop the output pointer entirely,
// storing with `stbx r8,<delta>,<address>`. Once that happens every register
// downstream is renamed and the score is 0 to 3 of 27.
//
// Measured, all with the delta transform:
//   * `out[i + 1] = ReadByte(c, addr + 1 + i)` at /O2   -- 3 of 27, 116 bytes
//   * the same at /O2 /Os                               -- 1 of 25, 108 bytes
//   * two explicit locals, `*d = ...; d++; a++;` at /O2 -- 0 of 27, 120 bytes
// The relationship the transform exploits holds in every spelling of the
// loop, so this is not reachable by reordering statements; it wants a
// mutation that changes register pressure, which is the gap
// tools/permuter.py still has.
//
// Two smaller facts that ARE settled and should not be re-derived: the
// doubled `clrlwi 24` then `clrlwi 28` needs the helper to return something
// wider than u8 with an explicit `(u8)` at the use, and `addi r9,r5,1`
// followed by `addi r9,r9,-1` means `out + 1` is a real expression evaluated
// BEFORE the count is known -- it is dead on the skip path.

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

static u32 ReadByte(MemCtx* c, u32 a)
{
    return c->dir->pages[a >> 12][a & 0xFFF];
}

u32 ReadRun(MemCtx* c, u32 addr, u8* out)
{
    u32 v = ReadByte(c, addr);
    out[0] = (u8)v;

    u32 n = g_runLen[(u8)v & 15];

    for (u32 i = 0; i < n; i++)
        out[i + 1] = (u8)ReadByte(c, addr + 1 + i);

    return n + 1;
}
