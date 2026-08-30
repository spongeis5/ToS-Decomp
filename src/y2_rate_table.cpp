// sub_825A39C8 -- look one of sixteen constants up by a byte code and write
// it through an out-parameter, returning 0, or 37 when the out pointer is
// null. 372 bytes, 3 callers, of which 64 are the jump table.
//
// The dispatch is the ordinary MSVC three-instruction form with the table
// laid immediately after the `bctr`:
//
//      clrlwi  r11,r4,24          the switch value is a BYTE
//      cmplwi  cr6,r11,15
//      bgt-    cr6,825A3B2C       out of range -> the default block
//      lis/addi r12 = 825A39FC ; rlwinm r0,r11,2,0,29
//      lwzx    r0,r12,r0 ; mtctr r0 ; bctr
//
// build/switch_tables.txt records 64 bytes of table at 825A39FC, and the
// entries are ABSOLUTE addresses, so the arms read straight out of it:
//
//      0 -> 825A3B2C (the default block)   1..15 -> 825A3A3C .. 825A3B1C
//
// so index 0 has no case of its own and falls to `default:` along with
// everything above 15. Every arm is the same three instructions --
//
//      li   r11,<constant> ; li r3,0 ; stw r11,0(r5) ; blr
//
// -- and the blocks appear in the image in ascending case order with the
// default LAST, which per MATCHED.md (measured on sub_827261D8) is source
// order, since MSVC lays case bodies out as written and does not invent
// groups.
//
// The first parameter is never read on any path. It is not `this`: a member
// function would put the byte code in r5 and the out pointer in r6, and both
// are one register lower here, so this is a free function whose first
// argument is unused.
//
// The null guard returns 37 and is written first, as a private
// `li r3,37 ; blr` reached by falling through the `bne-`, which is the
// branch polarity MATCHED.md records for a guard written before the body.
//
// AN UNBIASED JUMP TABLE MEANS CASE 0 HAS ITS OWN BODY, and this is worth
// recording because two spellings that look equivalent are not. The
// dispatch here is `cmplwi cr6,r11,15` with no `addi r11,r11,-1` in front
// of it and a SIXTEEN-entry table whose entry 0 is the default block.
//
//   * `case 1: ... case 15: ... default:` biases -- MSVC computes the
//     minimum live case as 1 and emits `addi r11,r11,-1 ; cmplwi cr6,r11,14`
//     with a fifteen-entry table. 69 of 93 words.
//   * `case 0: default: <body>` -- the two labels on ONE statement -- biases
//     IDENTICALLY, because MSVC drops any case label whose target is the
//     default block before it measures the range. Same 69 of 93.
//   * `case 0: <body> ... default: <body>` with the body written out TWICE,
//     case 0 last before the default, is 75 of 75 non-relocated words. The
//     two blocks are folded afterwards, so only one copy of
//     `li r11,8363 ; li r3,0 ; stw ; blr` appears in the image and the table
//     entry for 0 points at it.
//
// So the table's bias is evidence about the SOURCE's case list and not only
// about the value range: an unbiased table says case 0 was written, and a
// shared `case 0: default:` label cannot produce one.

#include "types.h"

struct RateSource;

int RateForCode(RateSource* src, u8 code, u32* out)
{
    (void)src;

    if (out == 0)
        return 37;

    switch (code)
    {
    case 1:  *out = 8413; return 0;
    case 2:  *out = 8463; return 0;
    case 3:  *out = 8529; return 0;
    case 4:  *out = 8581; return 0;
    case 5:  *out = 8651; return 0;
    case 6:  *out = 8723; return 0;
    case 7:  *out = 8757; return 0;
    case 8:  *out = 7895; return 0;
    case 9:  *out = 7941; return 0;
    case 10: *out = 7985; return 0;
    case 11: *out = 8046; return 0;
    case 12: *out = 8107; return 0;
    case 13: *out = 8169; return 0;
    case 14: *out = 8232; return 0;
    case 15: *out = 8280; return 0;
    case 0:  *out = 8363; return 0;
    default: *out = 8363; return 0;
    }
}
