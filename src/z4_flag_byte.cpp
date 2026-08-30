#include "types.h"

// sub_8215BCA0 -- read one global byte and return it. 12 B.
//
//      lis     r11,-32102        0x829A0000
//      lbz     r3,-18049(r11)    0x829A0000 - 0x4681 = 0x8299B97F
//      blr
//
// The address is ODD, so this is a stand-alone byte in .data, not a field of
// an aligned record.  `lbz` zero-extends straight into the return register
// and nothing normalises it afterwards -- there is no trailing
// `clrlwi r3,r3,24`.
//
// Both emitted non-blr words are relocated (the lis carries the HIGHA half
// and the lbz the LO half of one address), so 1 of 3 words is actually
// compared and the rest of the check is the shape: no frame, one load, one
// blr.
//
// Sits immediately after sub_8215BC20 (src/t2_scan_slots.cpp) and immediately
// before sub_8215BCB0 (src/l32_table_key.cpp), which are the same translation
// unit's cursor scan and table accessor; both are plain /O2.

extern u8 g_flag_8299B97F;

u8 GetFlag()
{
    return g_flag_8299B97F;
}
