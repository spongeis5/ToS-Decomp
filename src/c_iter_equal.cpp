// sub_8287E440 -- iterator comparison. 172 B, 45 callers.  /O2 /Os
//
// The two 52-byte blocks at 8287E440 and 8287E47C are the same code with r3
// replaced by r4, so one inlined predicate is applied to each operand:
//
//   lwz    r10,0(r3)      c = it->owner
//   cmplwi cr6,r10,0
//   beq-   cr6,TRUE
//   lwz    r11,0(r10)     b = c->bucket
//   cmplwi cr6,r11,0
//   beq-   cr6,TRUE
//   rotlwi r11,r11,0
//   lwz    r9,4(r3)       it->index
//   lwz    r11,4(r11)     b->last
//   cmpw   cr6,r9,r11     SIGNED
//   li     r11,0
//   ble-   cr6,DONE
//   TRUE:  li     r11,1
//   DONE:  clrlwi. r11,r11,24
//
// Three things had to be right at once, and each was visible in the listing:
//
// 1. The predicate is ONE `||` EXPRESSION, not a sequence of `if`s.
//    Written as `if (x) return true;` statements, MSVC materialises the final
//    comparison branchlessly -- subfc/eqv/rlwinm/addze/clrlwi, five
//    instructions -- and gives each early return its own `li r11,1 ; b`.
//    Written as one short-circuit expression the two early exits BRANCH INTO
//    the shared `li r11,1` and the last term is evaluated with a real compare,
//    which is exactly `cmpw / li 0 / ble- / li 1`. That difference is 2 words
//    per block, 4 over the function: 188 bytes against the target's 172.
//
// 2. `it->owner->bucket` is written out again in the third term rather than
//    held in a local. Common-subexpressioning the repeat is what produces
//    `rotlwi r11,r11,0` -- a register move to ITSELF, which no hand-written
//    statement asks for.
//
// 3. /Os, not /O2. Same source at /O2 is 180 bytes.
//
// The tail compares the same two fields the predicate touched, and r10 -- the
// first operand's `owner`, loaded in the very first instruction -- is still
// live 34 instructions later, so the load is shared with the predicate. That
// only happens if the predicate is inline.
//
//   lwz   r11,0(r4)
//   cmplw cr6,r10,r11     UNSIGNED: a pointer
//   bne-  cr6,FALSE
//   lwz   r11,4(r3)
//   lwz   r10,4(r4)
//   cmpw  cr6,r11,r10     SIGNED: an int
//   li    r11,1
//   beq-  cr6,OUT
//   FALSE: li r11,0
//   OUT:   clrlwi r3,r11,24

#include "types.h"

struct IterBucket
{
    u32 unk0000;
    int last;
};
ASSERT_OFFSET(IterBucket, last, 0x04);

struct IterOwner
{
    IterBucket* bucket;
};

struct Iterator
{
    IterOwner* owner;
    int        index;
};
ASSERT_OFFSET(Iterator, index, 0x04);

static bool IterAtEnd(const Iterator* it)
{
    return it->owner == 0
        || it->owner->bucket == 0
        || it->index > it->owner->bucket->last;
}

bool IterEqual(const Iterator* a, const Iterator* b)
{
    if (IterAtEnd(a) && IterAtEnd(b))
        return true;

    return a->owner == b->owner && a->index == b->index;
}
