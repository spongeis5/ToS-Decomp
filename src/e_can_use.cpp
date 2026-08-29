#include "types.h"

// sub_821FF908 -- a five-link pointer walk that answers TRUE when any link is
// missing, and otherwise tests three conditions on the object at the end.
// 168 B, 14 callers.  r3 = the root object, r4 = a key to match.
//
//   lwz r11,56(r3)   ; beq -> return 1
//   lwz r11,76(r11)  ; beq -> return 1
//   lwz r10,12(r11)  ; beq -> return 1      r10 = the slot, kept live
//   lwz r11,4(r10)   ; beq -> return 1      r11 = the entry
//   lwz r9,32(r11)   ; beq -> return 1
//   if (r4 && r4 != r11->16) return 0
//   if (r11->20 & 0x70)      return 0        rlwinm r8,r9,0,25,27
//   lwz r11,32(r11)                          <-- THE SAME FIELD, RELOADED
//   lfs f0,8(r10) ; lfs f13,16(r11) ; fcmpu ; blt-
//   lfs f13,12(r10) ; lfs f0,11684(r11) -> 82002DA4 = 0.0f ; fcmpu ; beq-
//   clrlwi r3,r11,24
//
// Every `beq-` jumps AWAY to the `li r3,1 ; blr` that sits at the very END of
// the body, so the interesting path is the fall-through: five nested `if`s
// with `return true` last, not five inverted early returns.
//
// THE RELOAD IS THE WHOLE FUNCTION. Written with both reads spelled
// `e->limit`, MSVC common-subexpression-eliminates them, keeps the pointer
// in r9 across the two intervening field loads, and emits 164 bytes -- 17 of
// 41 words, every instruction after the missing load shifted by one. Nine
// other spellings were tried on that shape and none of them moved it: naming
// the pointer in a local, not naming it, `&&` chains, nested ifs, a `goto`
// tail, a union at the same offset, reordering the two guards, an inline
// helper for the tail, and swapping the sides of the `>=`. Nor do flags: a
// 2304-combination sweep produced 168 bytes only under `/fp:precise`, and
// that extra word is a `bso-` NaN check, not this load.
//
// What produces it is spelling the SECOND read through the longer chain --
// `s->entry->limit` where the guard said `e->limit`. Same value, same
// register, but a different expression, so CSE does not fire, MSVC
// rematerialises the load, and r9 becomes free for the owner and flags reads.
// 40 of 40 non-relocated words.
//
// The tail is `clrlwi r3,r11,24`, a zero-extension to a byte, so the return
// type is bool. The five early returns are plain `li r3,1` with no
// extension -- the constant is already in range.

struct Limit { char pad0000[0x10]; f32 threshold; };
ASSERT_OFFSET(Limit, threshold, 0x10);

struct Entry
{
    char   pad0000[0x10];
    void*  owner;               /* 16 */
    u32    flags;               /* 20 */
    char   pad0018[0x08];
    Limit* limit;               /* 32 */
};
ASSERT_OFFSET(Entry, owner, 0x10);
ASSERT_OFFSET(Entry, flags, 0x14);
ASSERT_OFFSET(Entry, limit, 0x20);

struct Slot
{
    char   pad0000[0x04];
    Entry* entry;               /* 4 */
    f32    value;               /* 8 */
    f32    bias;                /* 12 */
};
ASSERT_OFFSET(Slot, entry, 0x04);
ASSERT_OFFSET(Slot, value, 0x08);
ASSERT_OFFSET(Slot, bias, 0x0C);

struct Table { char pad0000[0x0C]; Slot* slot; };
ASSERT_OFFSET(Table, slot, 0x0C);

struct Holder { char pad0000[0x4C]; Table* table; };
ASSERT_OFFSET(Holder, table, 0x4C);

struct Root { char pad0000[0x38]; Holder* holder; };
ASSERT_OFFSET(Root, holder, 0x38);

bool CanUse(Root* r, const void* key)
{
    Holder* h = r->holder;
    if (h)
    {
        Table* t = h->table;
        if (t)
        {
            Slot* s = t->slot;
            if (s)
            {
                Entry* e = s->entry;
                if (e)
                {
                    if (e->limit)
                    {
                        if (key != 0 && key != e->owner)
                            return false;
                        if (e->flags & 0x70)
                            return false;
                        return s->value >= s->entry->limit->threshold
                            && s->bias == 0.0f;
                    }
                }
            }
        }
    }
    return true;
}
