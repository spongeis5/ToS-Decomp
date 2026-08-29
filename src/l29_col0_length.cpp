// sub_82691B70 -- length of a 2x3 matrix's first column. 24 B, 3 callers.
//
//      lfs   f0,12(r3)
//      fmul  f0,f0,f0
//      lfs   f13,0(r3)
//      fmadd f0,f13,f13,f0
//      fsqrt f1,f0
//
// EVERY ARITHMETIC INSTRUCTION IS DOUBLE PRECISION -- fmul, fmadd, fsqrt,
// not the `s` forms -- while both loads are `lfs`.  That is a float member
// read into a double: `lfs` converts for free, so the source names doubles
// and the two multiplies never round to single.  A float expression would be
// fmuls/fmadds/fsqrts throughout.
//
// The offsets say which column.  Its sibling sub_82691B88 does the identical
// thing at +4 and +16, so the object has two rows of three floats -- 0,4,8
// and 12,16,20 -- and this pair takes elements {0,12} and {4,16}, the two
// COLUMNS.  That is the length of a 2D affine matrix's basis vectors, and it
// sits between src/e_mtx23_xform.cpp (826919A8) and src/e_mtx23_mul.cpp
// (82691C50), which are the same Mtx23.
//
// /O2 /Os, which is not a guess: both neighbours in that translation unit
// are /Os-only and adjacency is what decides the level.
//
// WHICH SQUARE BECOMES THE ADDEND IS DECLARATION ORDER, NOT SUM ORDER, and
// this pair of functions measured it against each other.  The target loads
// m10 first, squares it into the addend, and contracts m00's square into the
// fmadd.  Declaring `x = m00` first and writing `sqrt(x*x + y*y)` gives the
// two loads exchanged, 4 of 6; writing `sqrt(y*y + x*x)` with the same
// declarations changes NOTHING, still 4 of 6 -- the commutative float order
// is not source-readable, exactly as MATCHED.md says.  Declaring the addend's
// value FIRST is what moves it: 6 of 6.  Its sibling sub_82691B88 was
// compiled with the opposite change at the same time, so the two spellings
// were separated by measurement rather than by preference.

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
ASSERT_OFFSET(Mtx23, m10, 0x0C);
ASSERT_SIZE(Mtx23, 24);

double ColumnLength0(const Mtx23* m)
{
    double y = m->m10;
    double x = m->m00;

    return sqrt(x * x + y * y);
}
