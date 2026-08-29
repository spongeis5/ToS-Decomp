// sub_826C6298 -- walk a pointer array backwards and squeeze out the null
// entries. 124 B, 4 callers.
//
//      lhz    r11,4(r3)         count, a u16 at +4
//      addic. r8,r11,-1         i = count - 1, and set CR0 on the SIGNED result
//      bltlr                    the peeled test of the outer for
//      lis    r11,0
//      rlwinm r7,r8,2,0,29      i * 4, strength-reduced into an induction var
//      ori    r6,r11,65535      0x0000FFFF, hoisted out of the loop
// outer:
//      lwz    r11,0(r3)         items -- RELOADED every iteration
//      lwzx   r10,r7,r11
//      cmplwi cr6,r10,0
//      bne-   cr6,next
//      lhz    r10,4(r3)         count, read again
//      mr     r11,r8            j = i
//      add    r9,r10,r6         count + 0xFFFF
//      clrlwi r5,r9,16          truncated back to 16 bits
//      sth    r5,4(r3)          count = count - 1
//      cmpw   cr6,r8,r5         the peeled test of the inner for
//      bge-   cr6,next
// inner:
//      mr     r10,r7
//      lwz    r9,0(r3)          items -- RELOADED every iteration
//      addi   r11,r11,1
//      add    r9,r10,r9
//      addi   r10,r10,4
//      lwz    r5,4(r9)
//      stw    r5,0(r9)          items[j] = items[j+1]
//      lhz    r4,4(r3)          count -- RELOADED for the loop test
//      cmpw   cr6,r11,r4
//      blt+   cr6,inner
// next:
//      addic. r8,r8,-1
//      addi   r7,r7,-4
//      bge+   outer
//      blr
//
// BOTH LOOPS ARE ROTATED `for`s, not do/whiles: each has its test PEELED out
// in front (`bltlr` for the outer, `bge-` for the inner) and a second copy at
// the bottom. MATCHED.md's rule reads the other way -- a loop top reached by
// fall-through with no peeled copy is a do/while -- and this is the control
// for it.
//
// `items` IS RELOADED IN BOTH LOOPS and nothing here is a CSE-defeat lever:
// `items[j] = items[j+1]` can legally overwrite the `items` pointer itself if
// the array happens to start at the object, so the reload is real aliasing
// and it is what spelling `o->items[...]` at every use produces. `count` is
// reloaded for the same reason plus the `sth` in between.
//
// THE DECREMENT IS `count + 0xFFFF`, NOT `addi -1`, and the constant is
// hoisted out of the loop into r6. An `addi` immediate cannot be hoisted --
// it is part of the instruction -- so the constant is in a register only
// because it did not fit one: the addend is the UNSIGNED 65535 rather than
// -1, which is what 16-bit modular arithmetic on a `u16` produces, and
// `lis 0`/`ori 65535` is how a value that fits in neither `li` nor `addi`
// is built. The following `clrlwi ...,16` is there because the stored value
// is immediately re-read as the u16 `count` for the inner loop's peeled test.
//
// sub_826DB0A0 is this function again, word for word, at another address;
// see src/n13_compact_nulls2.cpp.
//
// Nothing is relocated; all 31 words are compared.

#include "types.h"

struct PtrVector
{
    /* 0x00 */ void** items;
    /* 0x04 */ u16    count;
};
ASSERT_OFFSET(PtrVector, count, 0x04);

void CompactNulls(PtrVector* o)
{
    for (int i = o->count - 1; i >= 0; i--)
    {
        if (o->items[i] == 0)
        {
            o->count = o->count - 1;
            for (int j = i; j < o->count; j++)
                o->items[j] = o->items[j + 1];
        }
    }
}
