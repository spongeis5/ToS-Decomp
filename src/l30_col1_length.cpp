// sub_82691B88 -- length of a 2x3 matrix's second column. 24 B, 3 callers.
//
//      lfs   f0,16(r3)
//      fmul  f0,f0,f0
//      lfs   f13,4(r3)
//      fmadd f0,f13,f13,f0
//      fsqrt f1,f0
//
// sub_82691B70 (src/l29_col0_length.cpp) word for word, one element along:
// {4, 16} instead of {0, 12}.  Two functions differing only by a matched
// pair of offsets is what says the object is a 2x3 matrix in row order and
// that these take its COLUMNS, rather than two unrelated pairs of floats.
//
// Double-precision arithmetic from `lfs` loads: the members are floats and
// the expression is double.  /O2 /Os, by adjacency to the Mtx23 routines
// either side.

#include "types.h"
#include <math.h>

struct Mtx23
{
    /* 0x00 */ f32 m00;
    /* 0x04 */ f32 m01;
    /* 0x08 */ f32 m02;
    /* 0x0C */ f32 m10;
    /* 0x10 */ f32 m11;
    /* 0x14 */ f32 m12;
};
ASSERT_OFFSET(Mtx23, m11, 0x10);
ASSERT_SIZE(Mtx23, 24);

double ColumnLength1(const Mtx23* m)
{
    double y = m->m11;
    double x = m->m01;

    return sqrt(x * x + y * y);
}
