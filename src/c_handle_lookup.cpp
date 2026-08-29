// sub_82662F20 -- two-level page-table lookup off a handle. 36 B, 28 callers.
//
//   lis    r11,-32091                      r11 = 0x82A50000
//   rlwinm r10,r3,15,17,28                 (h >> 20) * 8
//   rlwinm r9,r3,22,22,29                  ((h >> 12) & 0xFF) * 4
//   lwz    r11,16748(r11)                  r11 = g_pages        (0x82A5416C)
//   add    r11,r10,r11
//   lwz    r11,4(r11)                      page->slots
//   lwzx   r11,r11,r9                      slots[(h >> 12) & 0xFF]
//   lwz    r3,20(r11)
//
// Reading the two rlwinms:
//   MASK(17,28) = 0x00007FF8 and ROTL 15 puts source bits 20..31 at result
//   bits 3..14, so r10 is (h >> 20) scaled by 8 -- an index into an array of
//   8-byte elements.
//   MASK(22,29) = 0x000003FC and ROTL 22 puts source bits 12..19 at result
//   bits 2..9, so r9 is ((h >> 12) & 0xFF) scaled by 4 -- an index into an
//   array of pointers.
//
// Both strides come out of the shift-and-mask, so the 8-byte Page size is
// measured, not guessed.

#include "types.h"

struct HandleObject
{
    char unk0000[0x14];
    u32  value;
};
ASSERT_OFFSET(HandleObject, value, 0x14);

struct HandlePage
{
    u32            unk0000;
    HandleObject** slots;
};
ASSERT_OFFSET(HandlePage, slots, 0x04);
ASSERT_SIZE(HandlePage, 8);

extern HandlePage* g_handlePages;

u32 GetHandleValue(u32 h)
{
    return g_handlePages[h >> 20].slots[(h >> 12) & 0xFF]->value;
}
