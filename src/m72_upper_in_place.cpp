// sub_8215A8F0 -- upper-case a string in place. 64 bytes, 3 callers.
//
//      lbz    r11,0(r3) ; mr r10,r3
//      cmplwi cr6,r11,0 ; beqlr cr6
//  L:  lbz    r11,0(r10) ; extsb r11,r11
//      cmpwi  cr6,r11,97  ; blt- cr6,<skip>
//      cmpwi  cr6,r11,122 ; bgt- cr6,<skip>
//      addi   r11,r11,-32
//  skip:
//      stb    r11,0(r10)
//      lbzu   r9,1(r10) ; cmplwi cr6,r9,0 ; bne+ cr6,L
//      blr
//
// THE SIGNEDNESS IS SPLIT WITHIN THE LOOP, the same split
// src/c_hash_upper.cpp (sub_8215A420) needed: `cmplwi` on the RAW byte where
// the loop tests it, `extsb` plus `cmpwi` on a `char` inside the fold. That
// is a `const`-free `u8*` walked by the loop with `char c = (char)*s;` taken
// fresh inside the body.
//
// The character is loaded TWICE per iteration -- once by the `lbzu` at the
// bottom for the loop test, and again by the `lbz` at the top for the fold --
// and the reload is not a missed CSE: the `stb` in between writes the very
// byte the next `lbz` reads, so MSVC cannot forward it.
//
// 97 and 122 are 'a' and 'z'; the fold is a plain subtraction of 32 rather
// than the bit trick sub_8215A420 uses, and the two `cmpwi` with `blt-`/`bgt-`
// out are a range test written as one `&&`.
//
// `mr r10,r3` again means the walk runs on its own local: consuming the
// parameter lets MSVC use r3 in place and the copy disappears.
//
// Nothing is relocated: 16 of 16 words are compared.
//
// NEAR MISS: 1 of 16 at /O2, 68 bytes against 64, and ONE extra instruction
// explains all of it. Retail keeps the SIGN-EXTENDED character as the single
// merged value -- `extsb r11,r11` in place, `addi r11,r11,-32` in place,
// `stb r11,0(r10)` -- so both arms of the range test arrive at the store in
// the same register. We extend into a second register and leave the RAW byte
// as the merge value, which is legal (on the untaken path `(u8)c == *p`) and
// then costs an `extsb r10,r10` on the taken path to convert back. Every
// register after that is renamed, which is why the word count reads 1 of 16
// rather than 2.
//
// Measured, all the same:
//   * `c = (char)(c - 32)` and `c -= 32` -- identical, the cast is not it
//   * an inlined `static char ToUpper(char)` returning the folded character,
//     which is the shape that should force the merge onto the extended value
//     -- folded straight back, byte for byte the same object
//   * /O2 /Os -- the right LENGTH, 64 bytes, but it rotates the loop
//     differently: a `b` into the bottom test instead of retail's
//     `cmplwi`/`beqlr` guard at the top. Worse structurally, so /O2 is the
//     level and the residue is not the flag.
//
// The signedness split itself is settled and should not be re-derived: an
// all-unsigned body loses the `extsb` and both `cmpwi`, and an all-signed
// walk turns the loop test into `cmpwi` where retail has `cmplwi`.

#include "types.h"

static char ToUpper(char c)
{
    if (c >= 'a' && c <= 'z')
        c -= 32;

    return c;
}

void UpperInPlace(u8* s)
{
    u8* p = s;

    while (*p != 0)
    {
        *p = (u8)ToUpper((char)*p);
        ++p;
    }
}
