#include "types.h"

// sub_82287E80 -- two-level search: over an array of slot pointers at +0xE4 of
// the object, and inside each slot over an 8-byte record array, for the record
// whose first word equals the key. 160 B, 10 callers.
//
//      lis     r11,-32108
//      li      r8,0             i = 0
//      lwz     r11,-24180(r11)  g_world  (a global POINTER read: 0x8293A18C)
//      lwz     r6,1080(r11)     n = g_world->count
//      cmpwi   cr6,r6,0
//      ble-    cr6,0x82287EE8   -> li r3,0 ; blr
//      addi    r7,r3,228        &o->slots[0]
//  L1: lwz     r10,0(r7)        e = o->slots[i]
//      cmplwi  cr6,r10,0
//      beq-    cr6,L3
//      lwz     r9,64(r10)       m = e->count
//      li      r11,0            j = 0
//      cmplwi  cr6,r9,0
//      beq-    cr6,L3
//      lwz     r10,68(r10)      it = e->items
//  L2: lwz     r5,0(r10)
//      cmpw    cr6,r5,r4
//      beq-    cr6,found
//      addi    r11,r11,1
//      addi    r10,r10,8        8-byte stride
//      cmplw   cr6,r11,r9       UNSIGNED: j and m are u32
//      blt+    cr6,L2
//  L3: addi    r8,r8,1
//      addi    r7,r7,4
//      cmpw    cr6,r8,r6        SIGNED: i and n are s32
//      blt+    cr6,L1
//      li      r3,0 ; blr
//  found:
//      addi    r10,r8,57        228 + 4*i folded as 4*(57 + i)
//      rlwinm  r11,r11,3,0,28   j * 8
//      rlwinm  r9,r10,2,0,29
//      lwzx    r8,r9,r3         o->slots[i] RELOADED from the object
//      lwz     r10,68(r8)       ->items
//      add     r7,r10,r11
//      lbz     r6,4(r7)         ->[j].flag, a BYTE at +4
//      cntlzw  r5,r6
//      rlwinm  r4,r5,27,31,31   (flag == 0)
//      xori    r11,r4,1         (flag != 0)
//      addi    r3,r11,1
//      blr
//
// Read off the listing:
//   * `lis` feeding a `lwz` with a signed displacement is a read OF a global
//     pointer variable, not the address of a global object.
//   * the found block recomputes the slot address from `this` and the index
//     rather than reusing the induction pointer, which is what spelling
//     `o->slots[i]` a second time gives -- 228 folds into the 57.
//   * `cmpw`, not `cmplw`, on the key: the key is SIGNED on both sides.
//   * both `beq-` guards jump FORWARD to the loop increment, so the
//     interesting path is the fall-through and the guards are nested.
//
// Three things had to be read rather than guessed, each worth a word or more:
//
//   * `cntlzw / rlwinm 27,31,31 / xori 1 / addi 1` is `(flag == 0) ? 1 : 2`.
//     Spelling the same value as `(flag != 0) + 1` gets the OTHER branchless
//     idiom -- `addic r5,r6,-1 ; subfe` -- which is one word shorter, so the
//     function came out 156 bytes against 160. The compiler computes the
//     comparison the source wrote and inverts it; it does not canonicalise.
//   * `li r11,0` for j is emitted BEFORE the `m == 0` test, which forces j and
//     the slot pointer into different registers (r11 and r10). With j declared
//     by a `for` inside the guard MSVC sinks the initialisation below the
//     `lwz r10,68(...)`, lets j reuse the slot's register, and displaces four
//     words. Declaring `u32 j = 0;` ahead of `if (m != 0)` restores it.
//   * the inner loop is a guard plus a do/while: its top is a branch target
//     reached by falling in, with the only copy of the test at the bottom.

struct SlotItem
{
    /* 0x00 */ s32  key;
    /* 0x04 */ u8   flag;
    /* 0x05 */ u8   unk0005[3];
};

ASSERT_OFFSET(SlotItem, key,  0x00);
ASSERT_OFFSET(SlotItem, flag, 0x04);
ASSERT_SIZE(SlotItem, 8);

struct Slot
{
    /* 0x00 */ char      unk0000[0x40];
    /* 0x40 */ u32       count;
    /* 0x44 */ SlotItem* items;
};

ASSERT_OFFSET(Slot, count, 0x40);
ASSERT_OFFSET(Slot, items, 0x44);

struct SlotWorld
{
    /* 0x000 */ char unk0000[0x438];
    /* 0x438 */ s32  count;
};

ASSERT_OFFSET(SlotWorld, count, 0x438);

extern SlotWorld* g_slotWorld;

struct SlotOwner
{
    /* 0x000 */ char  unk0000[0xE4];
    /* 0x0E4 */ Slot* slots[64];
};

ASSERT_OFFSET(SlotOwner, slots, 0xE4);

s32 FindSlotItem(SlotOwner* o, s32 key)
{
    s32 n = g_slotWorld->count;

    for (s32 i = 0; i < n; ++i)
    {
        Slot* e = o->slots[i];
        if (e)
        {
            u32 m = e->count;
            u32 j = 0;
            if (m != 0)
            {
                SlotItem* it = e->items;
                do
                {
                    if (it->key == key)
                        return o->slots[i]->items[j].flag == 0 ? 1 : 2;
                    ++j;
                    ++it;
                }
                while (j < m);
            }
        }
    }

    return 0;
}
