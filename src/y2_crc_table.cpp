// sub_8215CCC0 -- build the 256-entry CRC-32 table, once. 240 bytes,
// 1 caller.
//
// The polynomial is materialised as `lis r11,1217 ; ori r11,r11,7607` =
// 0x04C11DB7, the standard reversed-input CRC-32 constant, and the table is
// the global array whose address `lis`/`addi` resolves to 8299B9A8. The
// guard flag is read and written through ONE `lis`, as `lwz r11,21092(r7)`
// and `stw r11,21092(r7)`, which per src/b_fwd_global5.cpp's note is a
// global VARIABLE rather than the address of a global object.
//
//      lwz    r11,21092(r7) ; cmpwi cr6,r11,0 ; beq-   signed test -> `int`
//      li     r11,256 ; mtctr r11                      the outer trip count
//      li     r9,0                                     i, a second induction
//      addi   r8,r10,-4                                the table, biased for
//                                                      the `stwu`
//
// The eight bit steps are FULLY UNROLLED by the compiler and each is
//
//      rlwinm r6,r10,0,0,0        the sign bit, tested in place
//      rlwinm r10,r10,1,0,30      crc << 1, computed for BOTH arms
//      cmplwi cr6,r6,0 ; beq- +8
//      xor    r10,r10,r11
//
// except the first, which folds the initial `(u32)i << 24` into its own two
// `rlwinm`s: `rlwinm r6,r9,24,0,0` is the sign bit of `i << 24` and
// `rlwinm r10,r9,24,0,7` is the shift itself, masked to the byte that
// survives it.
//
// Both exits return -1, so the value carries no information about which path
// ran; `li r3,-1 ; blr` sits after the store that clears the flag and is
// also the early exit's target.

#include "types.h"

extern int g_crcTableDirty;      /* 82935264 */
extern u32 g_crcTable[256];      /* 8299B9A8 */

int BuildCrcTable()
{
    if (g_crcTableDirty != 0)
    {
        for (int i = 0; i < 256; i++)
        {
            u32 crc = (u32)i << 24;

            for (int j = 0; j < 8; j++)
            {
                if (crc & 0x80000000u)
                    crc = (crc << 1) ^ 0x04C11DB7u;
                else
                    crc = crc << 1;
            }

            g_crcTable[i] = crc;
        }

        g_crcTableDirty = 0;
    }

    return -1;
}
