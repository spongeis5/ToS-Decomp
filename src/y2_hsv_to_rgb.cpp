// sub_827C35F0 -- integer HSV to RGB. 316 bytes, no direct callers.
//
// The constants name the algorithm. 15300 is 255*60, 30600 is 2*15300 and
// 7650 is 15300/2, the rounding term; 510 is 2*255. The sector arithmetic is
// `h / 60` and `h % 60` with `h` first folded into 0..359 by `h % 360`, and
// the six arms assign exactly the standard permutation:
//
//      0 (v, t, p)   1 (q, v, p)   2 (p, v, t)
//      3 (p, q, v)   4 (t, p, v)   5 (v, p, q)
//
// where p is the same in every arm and the third value is computed with
// `h % 60` on the ODD sectors and `60 - h % 60` on the even ones -- which is
// what the single `clrlwi. r30,r11,31` on the sector splits the function in
// two for.
//
// `divwu` on the two rounded divisions and `divw` on `h / 360` and `h / 60`
// says the saturation and value are UNSIGNED and the hue is a signed `int`;
// `cmpwi cr6,r4,-1 ; ble-` confirms the hue, and it also fixes the spelling
// of the guard as `h > -1` rather than `h >= 0`, which would compare against
// 0 and use `blt-`.
//
// `h / 60` is computed TWICE, into r9 for the remainder and into r11 for the
// sector, with nothing in between -- per MATCHED.md a reload with no store
// between it and the first read means the source wrote the two differently,
// here as `h / 60` and `h % 60`.
//
// The three outputs live in r8, r7 and r6 and are stored to +1, +2 and +3;
// r6 is the incoming `v` and is never moved for the arms that leave the blue
// channel alone, which is why `b` needs no `mr` in sectors 3 and 4.
//
// Four blocks are TAIL-MERGED across the odd/even split: sector 5 jumps into
// sector 2's last move and sector 3 into sector 0's, so the last assignment
// of each of those arms is shared. That is what fixes the order the three
// assignments are written in inside each arm.

#include "types.h"

struct ColorARGB
{
    /* 0x00 */ u8 a;
    /* 0x01 */ u8 r;
    /* 0x02 */ u8 g;
    /* 0x03 */ u8 b;
};
ASSERT_OFFSET(ColorARGB, r, 0x01);
ASSERT_OFFSET(ColorARGB, g, 0x02);
ASSERT_OFFSET(ColorARGB, b, 0x03);

void HsvToRgb(ColorARGB* out, int h, u32 s, u32 v)
{
    u32 r = v;
    u32 g = v;
    u32 b = v;

    if (s != 0 && h > -1)
    {
        if (h >= 360)
            h = h % 360;

        u32 p = ((255 - s) * v * 2 + 255) / 510;
        int sector = h / 60;
        int f = h % 60;

        if ((sector & 1) != 0)
        {
            u32 q = ((15300 - f * s) * v + 7650) * 2 / 30600;

            if (sector == 1)
            {
                g = v;
                r = q;
                b = p;
            }
            else if (sector == 3)
            {
                r = p;
                g = q;
            }
            else if (sector == 5)
            {
                r = v;
                g = p;
                b = q;
            }
        }
        else
        {
            u32 t = ((15300 - (60 - f) * s) * v + 7650) * 2 / 30600;

            if (sector == 0)
            {
                r = v;
                b = p;
                g = t;
            }
            else if (sector == 2)
            {
                g = v;
                r = p;
                b = t;
            }
            else if (sector == 4)
            {
                r = t;
                g = p;
            }
        }
    }

    out->r = (u8)r;
    out->g = (u8)g;
    out->b = (u8)b;
}
