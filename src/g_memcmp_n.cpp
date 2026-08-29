#include "types.h"

// sub_82697748 -- counted byte compare, 60 B. (The .pdata entry that covers it
// starts 8 bytes earlier at 82697740, where an 8-byte `b 0x828A8C50` thunk
// sits; 82697730 and 82697738 are two more of the same, so the unwind record
// simply spans the thunk run and the function behind it.)
//
//      mr      r11,r3
//      mr      r10,r4
//      li      r3,0             the result register is zeroed before the guard
//      cmplwi  cr6,r5,0
//      beqlr   cr6
//      add     r9,r11,r5        end = a + n, computed after the guard
//  L:  lbz     r8,0(r11)
//      lbz     r7,0(r10)
//      subf.   r3,r7,r8         r8 - r7, i.e. *a - *b; sets cr0
//      bnelr                    a difference returns it immediately
//      addi    r11,r11,1
//      addi    r10,r10,1
//      cmpw    cr6,r11,r9
//      bne+    cr6,L
//      blr
//
// Both bytes arrive by lbz with no extsb, so the subtraction is on
// zero-extended bytes -- unsigned char, which is what memcmp requires.
//
// The loop top is a branch target reached by fall-through with no peeled copy
// of the bound test ahead of it, so the loop is a do/while under a guard.
//
// NOT MATCHED, and match.py cannot even be pointed at it: 82697748 is not a
// recorded function start (build/functions_all.txt has 82697740 68), so
// match.py exits 1 before compiling anything. The figures below come from a
// direct word comparison against the image using the same peimage/libmatch
// modules match.py uses, at /O2 /Gy /GS- /fp:fast.
//
// 14 word(s) compared: 2 identical, 12 differ, 0 relocated -- and the 12 are
// ONE missing instruction, not twelve decisions. Ours is 56 bytes to the
// target's 60 because the target copies `b` into a scratch (`mr r10,r4`) and
// puts `end` in r9, where we leave `b` in r4 and put `end` in r10; everything
// after the prologue is then off by one word. Modulo that shift the two agree
// instruction for instruction except for the order of the two `lbz`.
//
// Three source shapes moved this and are worth keeping:
//   * `d = *a - *b; if (d) break;` rather than `return d` -- the early return
//     lets MSVC prove the tail `return d` is zero and it re-materialises
//     `li r3,0` at the BOTTOM. With `break` the value stays live in r3 and the
//     zero moves to the top, which is where the target has it.
//   * `if (n != 0) { ... } return d;` rather than `if (n == 0) return d;` --
//     the same lever as src/g_find_by_key.cpp: one return point folds the
//     branch into `beqlr`, two return points emit `bne-` plus a local `blr`.
//   * `(s32)a != (s32)end` -- the pointer compare is SIGNED (`cmpw`); without
//     the cast MSVC emits `cmplw`.
// Copying a and b into walker locals is a dead end: MSVC strength-reduces the
// second pointer to a base difference and the loop becomes `lbzx`.
// /O2 /Os changes nothing here.

int MemCmpN(const u8* a, const u8* b, u32 n)
{
    int d = 0;

    if (n != 0)
    {
        const u8* end = a + n;
        do
        {
            d = *a - *b;
            if (d)
                break;
            ++a;
            ++b;
        } while ((s32)a != (s32)end);
    }
    return d;
}
