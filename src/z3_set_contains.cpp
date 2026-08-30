// sub_825FEF80 -- linear search of a 16-entry array for a key, returning
// whether it was found.  72 B, 1 caller (82250420).  BRIDGE: between
// k_sorted_insert (825FEF00) and r1_list_remove (825FEFC8), both matched.
//
//      lwz    r9,64(r3)          the count, read ONCE
//      mr     r10,r3             the induction pointer -- items are at +0
//      li     r11,0              the index
//      cmpwi  cr6,r9,0 ; ble-    SIGNED: the count is an s32
//   L: lwz    r8,0(r10)
//      cmplw  cr6,r4,r8 ; beq-   UNSIGNED: the elements are u32
//      addi   r11,r11,1 ; addi r10,r10,4
//      cmpw   cr6,r11,r9 ; blt+ L
//      subfc  r10,r9,r11         i - n, for the CARRY only
//      eqv    r9,r9,r11          ~(n ^ i): its sign bit is "same sign"
//      rlwinm r8,r9,1,31,31      that sign bit
//      addze  r7,r8
//      clrlwi r3,r7,31
//
// The tail is the branchless SIGNED `i < n`.  With equal signs the unsigned
// carry already answers it and the +1 inverts it; with opposite signs the
// carry alone is the answer.  The `subfc`'s difference is never read -- it
// is there for the carry, exactly as MATCHED.md records for this idiom.
//
// Guard plus do/while with both an index and an induction pointer live is
// what MSVC makes of an ordinary counted `for` with a `break`.
//
// THE RETURN MUST RE-READ `s->count`, NOT USE THE LOCAL.  With `return i < n`
// MSVC relates the comparison to the loop it just left: the fall-through
// exit means i == n, so it constant-folds that arm to `li r11,0` and gives
// each exit its own block -- 84 bytes, and only the loop's 10 words of 18
// right.  Reading the field again breaks that relation, both exits merge
// into the one block the image has, and the comparison is materialised.
// There is no reload: nothing stores in between, so the second read is
// common-subexpressioned back into r9.  This is MATCHED.md's CSE lever run
// backwards -- there a longer spelling FORCED a reload, here re-reading the
// field costs nothing and only removes what the compiler had inferred.
//
// WHAT THE BYTES DO NOT DECIDE.  Measured over 240 combinations: six loop
// spellings (for/break, `while (i < n && key != items[i])`, while with a
// break, an explicit guard plus do/while, a goto form, and an induction
// pointer) and four return types (`bool`, `int`, `u32`, `u8`) -- ALL 24
// combinations with the re-read are byte-identical and match; all 216
// without it fail identically.  So neither the loop spelling nor the return
// type is readable off this function, and `bool` here is a choice, not a
// measurement.  `i != n`, `n > i` and a materialised `unsigned r > 0` all
// fail.  /O2 only: /Os is 12 of 18.

#include "types.h"

struct Set
{
    /* 0x00 */ u32 items[16];
    /* 0x40 */ s32 count;
};
ASSERT_OFFSET(Set, count, 0x40);

bool Contains(const Set* s, u32 key)
{
    s32 n = s->count;
    s32 i;

    for (i = 0; i < n; i++)
    {
        if (key == s->items[i])
            break;
    }

    return i < s->count;
}
