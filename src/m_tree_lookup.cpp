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
// MATCHED, 11 of 11 words.
//
// THE ANSWER: THE LOOP EXIT TEST IS WRITTEN IN EVERY ARM, NOT AS THE LOOP
// CONDITION. This file used to say the layout was unreachable -- 7 of 11,
// every instruction correct, the size exact, and only two branch
// DESTINATIONS wrong, because `return 0` came after the out-of-line left
// block instead of before it. Five loop shapes gave the same two offsets.
//
// What none of them changed is where the JOIN sits. Written as
// `while (n != 0) { ... }` the loop's latch is a block of its own, so MSVC
// emits `b join` at the bottom of the right arm, lays the left arm down
// next, then the join, then the shared `return 0` -- and the zero return
// ends up last. Written with `if (n == 0) return 0;` at the END OF EACH ARM
// and no loop condition at all, MSVC merges the two tests into one latch,
// merges all three `return 0`s into one block, and plants that block
// IMMEDIATELY AFTER the latch, which pushes the left arm past it. The right
// arm then falls straight into the latch and its `b join` disappears -- the
// same 19 instructions in the target's order.
//
// So: WHEN A LOOP'S EXIT TEST IS EMITTED AS THE FALL-THROUGH OF ONE ARM
// WITH THE OTHER ARM OUT OF LINE BEHIND THE EXIT BLOCK, the source tested
// the exit condition inside the arms rather than in the loop header. The
// tell is that the exit block sits BETWEEN the latch and the out-of-line
// arm; a loop-condition spelling always puts the arm first.
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
    if (n == 0)
        return 0;

    for (;;)
    {
        if (key >= n->key)
        {
            if (key <= n->key)
                return NodeValue(n);
            n = (TreeNode*)((u32)n->right & ~3u);
            if (n == 0)
                return 0;
        }
        else
        {
            n = n->left;
            if (n == 0)
                return 0;
        }
    }
}
