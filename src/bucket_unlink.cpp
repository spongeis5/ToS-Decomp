// sub_82252C98 -- unlink a node from one bucket of a chained hash table,
// 80 bytes, 3 callers, no float ops.
//
//      addi    r11,r4,7            index + 7, BEFORE the shift
//      rlwinm  r10,r11,2,0,29      * 4
//      lwzx    r11,r10,r3          the bucket head
//      cmplw   cr6,r11,r5
//      bne-    cr6,...             not the head: walk
//      lwz     r11,8(r11)
//      stwx    r11,r10,r3          head case: bucket = head->next
//      blr
//
// The `addi ... ,7` before the shift is the whole layout statement. MSVC
// folds a constant byte offset into the INDEX when the element size is the
// scale it is already applying, so `t->buckets[i]` with buckets at +0x1C
// becomes `*(base + (i + 7) * 4)`. An array at offset 0 would emit the
// shift alone, and a separate `addi r10,r10,0x1C` after the shift would
// mean the offset was added to the ADDRESS -- a different expression.
//
// So the bucket array starts at 0x1C, and `next` is at 8 in the node.
//
// The head case stores `r11`, which is the value loaded from the BUCKET,
// not from r5. Both hold the same pointer there, so the two spellings are
// interchangeable in meaning but not in bytes: reading it back off the
// node re-loads r5 and emits a different register.
//
// Three exits and one loop, all through cr6, and every comparison is
// UNSIGNED (`cmplw`, `cmplwi`) -- pointers, not integers.
//
// THE LOOP HAS TO BE A `while`, and this cost two attempts. Written as
// `if (cur == 0) return;` followed by `for (;;)`, MSVC strength-reduces the
// walk onto the FIELD: it emits `addi r10,r11,8` to keep `&cur->next` live
// and the loop comes out three words longer. Moving the store out of the
// loop with a `break` changes nothing -- both spellings emit the same
// eighty-nine bytes.
//
// The tell is in the target: the back-edge is `bne+`, predicted taken, and
// the value it tests is r10 -- the pointer just assigned, not the one the
// body loaded from. That is a rotated `while (cur != 0)`, whose entry test
// MSVC hoists above the loop and whose exit test lands on the new value
// while it is still in a register. Spelled that way the address never
// becomes a variable and the `addi` never appears.

#include "types.h"

struct Node
{
    /* 0x0000 */ char  unk0000[0x8];
    /* 0x0008 */ Node* next;
};

ASSERT_OFFSET(Node, next, 0x8);

struct Table
{
    /* 0x0000 */ char  unk0000[0x1C];
    /* 0x001C */ Node* buckets[1];
};

ASSERT_OFFSET(Table, buckets, 0x1C);

void Unlink(Table* table, int index, Node* node)
{
    Node* cur = table->buckets[index];

    if (cur == node)
    {
        table->buckets[index] = cur->next;
        return;
    }

    while (cur != 0)
    {
        Node* next = cur->next;

        if (next == node)
        {
            cur->next = node->next;
            return;
        }

        cur = next;
    }
}
