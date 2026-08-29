#include "types.h"

// sub_8217E808 -- look a 64-bit key up in a balanced tree. 76 B, 116 callers.
//
// SECOND RUNG OF THE CLIMB. Its one callee, 8261B2F8, is already matched
// (src/k_flag_vcall.cpp), so again every name was decided before this was
// written.
//
//      lis     r11,-32092
//      addi    r10,r11,-29476  ; = 82A38CDC
//      lwz     r11,4(r10)      root
//      cmplwi  cr6,r11,0
//      beq-    cr6,none
// loop:ld      r10,0(r11)      the key -- SIXTY-FOUR bits
//      cmpld   cr6,r3,r10      and an unsigned 64-bit compare
//      blt-    cr6,left
//      ble-    cr6,found       not less and not greater: equal
//      lwz     r11,36(r11)     right
//      rlwinm  r11,r11,0,0,29  ... with the low TWO BITS masked off
// join:cmplwi  cr6,r11,0
//      bne+    cr6,loop
// none:li      r3,0 ; blr
// left:lwz     r11,32(r11)     left, NOT masked
//      b       join
// found:mr     r3,r11
//      b       0x8261B2F8
//
// THE MASK IS THE INTERESTING PART. The right child is loaded and then has
// its low two bits cleared; the left child is not. That is a tagged pointer
// -- the node's colour, or a thread flag, packed into the right pointer,
// which is free because nodes are at least four-byte aligned. A tree that
// stored the tag in its own field would not need the mask and would be four
// bytes bigger per node.
//
// `ld` and `cmpld` say the key is genuinely 64 bits, not a pointer: a
// pointer on this target is 32 bits in a 64-bit register and would be
// compared with `cmplw`.
//
// THE NESTING IS READABLE OFF THE LAYOUT. `blt-` jumps AWAY to the left
// child and `right` is the fall-through, which is not what
// `if (<) left; else if (==) found; else right;` produces -- that puts LEFT
// inline and scores 4 of 11. The target is the nested form: an outer
// `if (key >= n->key) { ... } else n = n->left;` with the else out of line.
//
// And the inner test is written `<=`, not `==`. Once `>=` is known, `<=`
// means equal, and MSVC emits `ble-` reusing cr6 where `==` emits a fresh
// `beq-`. One word, and the source really does say `<=`.
//
// NOT MATCHED -- 7 of 11 compared, but EVERY INSTRUCTION IS CORRECT and the
// size is exact at 76 bytes. The only differences are the two branch
// DESTINATIONS: the target places `return 0` before the out-of-line left
// block and this places it after. Five loop shapes were tried -- while with
// else, for(;;) with the null test at the top, `continue` in either arm, and
// the null test hoisted ahead of a do/while -- all seven of eleven with the
// same two offsets, and /O2 /Os is five. It is a block-placement choice.
//
// The recorded size is 44, not 76: 8217E834 is listed as a function start
// and control falls into it from the `rlwinm` above. The join of the two
// child assignments, taken for a tail call by the branch sweep -- the shape
// FINDINGS 7u measures.
struct TreeNode
{
    /* 0x00 */ u64       key;
    /* 0x08 */ char      unk0008[0x20 - 0x08];
    /* 0x20 */ TreeNode* left;
    /* 0x24 */ TreeNode* right;
};
ASSERT_OFFSET(TreeNode, left, 0x20);
ASSERT_OFFSET(TreeNode, right, 0x24);

struct TreeRoot
{
    char      unk0000[4];
    TreeNode* root;
};
ASSERT_OFFSET(TreeRoot, root, 4);

extern TreeRoot g_keyTree_82A38CDC;

int NodeValue(TreeNode* n);

int LookupKey(u64 key)
{
    TreeNode* n = g_keyTree_82A38CDC.root;
    while (n != 0)
    {
        if (key >= n->key)
        {
            if (key <= n->key)
                return NodeValue(n);
            n = (TreeNode*)((u32)n->right & ~3u);
        }
        else
        {
            n = n->left;
        }
    }
    return 0;
}
