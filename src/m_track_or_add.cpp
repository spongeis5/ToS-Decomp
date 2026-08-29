#include "types.h"

// sub_82602EA0 -- look something up in one table, and if it is not there,
// add it to another. 104 B, 240 callers.
//
//      mflr    r12 ; bl 0x828A75CC (__savegprlr_29) ; stwu r1,-112(r1)
//      mr      r31,r3 ; mr r30,r5 ; mr r29,r6
//      cmplwi  cr6,r3,0
//      bne-    cr6,go
//      addi    r1,r1,112 ; b 0x828A761C            return
// go:  lis     r11,-32093
//      mr      r5,r30
//      addi    r3,r11,26060       ; = 82A365CC
//      mr      r4,r31
//      bl      0x825FCA68         Find(&g_found, a, c)
//      cmplwi  cr6,r3,0
//      bne-    cr6,out
//      lis     r11,-32093
//      mr      r7,r29
//      addi    r3,r11,26264       ; = 82A36698
//      li      r6,16
//      mr      r5,r30
//      mr      r4,r31
//      bl      0x82602C60         Add(&g_pending, a, c, 16, d)
// out: addi    r1,r1,112 ; b 0x828A761C
//
// THE SECOND PARAMETER IS NEVER READ. r4 comes in and nothing touches it --
// r3, r5 and r6 are saved to r31/r30/r29 and r4 is not. That is not a
// transcription error and it is not a member function either (r3 is passed
// on as an ordinary argument, which `this` also would be). It is a
// parameter the release build does not use, which is what a name or a
// source location argument looks like once the logging is compiled out.
//
// IT RETURNS A VALUE, and that is what the TWO epilogues say. The target
// emits `addi r1,r1,112 ; b __restgprlr_29` twice -- once for the null guard
// and once at the end -- where a void version branches to a single shared
// exit and comes out 92 bytes against 104. Written as `return 0;` with r3
// already holding the null key, MSVC emits the epilogue in place rather than
// jumping to it. The end value is Find's result, or Add's when Find returned
// nothing, which is what makes the two exits carry different values and
// stops them being merged.
//
// The two globals are 204 bytes apart and are passed as the first argument
// to two different routines, so they are two containers rather than one
// object read at two offsets -- each gets its own lis/addi pair, and two
// offsets into one object would have shared a base register the way
// c_share_static.cpp's +4 does.
struct Registry;
extern Registry g_found_82A365CC;
extern Registry g_pending_82A36698;

void* FindEntry(Registry* r, void* key, void* ctx);
void* AddEntry(Registry* r, void* key, void* ctx, int kind, void* extra);

void* TrackOrAdd(void* key, void* unused, void* ctx, void* extra)
{
    if (key == 0)
        return 0;

    void* e = FindEntry(&g_found_82A365CC, key, ctx);
    if (e == 0)
        e = AddEntry(&g_pending_82A36698, key, ctx, 16, extra);
    return e;
}
