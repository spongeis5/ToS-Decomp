// sub_825478F8 -- walk an intrusive circular list looking for a key; hand the
// item back through an out-parameter and return a status. 116 B, 5 callers.
//
//   cmplwi cr6,r5,0 ; bne- <body> ; li r3,37 ; blr
//
// `return 37` is the FALL-THROUGH of the test, so the guard is written first
// and positively: `if (out == 0) return 37;`.
//
//   lwz  r11,352(r3)
//   cmplwi cr6,r11,0 ; addi r11,r11,-92 ; bne- ; li r11,0
//
// is the base-class adjustment from the idiom table, run backwards: the `-92`
// converts a link pointer into the item that contains it, and the null test
// exists only so a null link stays null. It appears twice, once for the head
// and once for each `next`, which is `static_cast<Item*>(...)` on a `Link`
// base sitting at offset 92 of `Item`.
//
// `addi r10,r3,260` is the SENTINEL, and it carries NO null test, which is
// the whole of what decides the head's type. Written as
// `static_cast<Item*>(&r->head)` with `head` a bare `Link` at +352, MSVC
// cannot prove `r + 352` non-null and emits the check anyway --
// `addic. r11,r3,352 ; bne- ; li r10,0` -- three words too many, measured.
// The head is therefore a whole `Item` at +260, and 260 + 92 = 352 is where
// its own `next` lives, which is exactly the word the loop starts from.
// One layout produces both constants and no check.
//
// The loop is `cmplw`/`beq-` out at the top and `cmplw`/`bne+` at the bottom,
// MSVC's rotation of `for (p = first; p != end; p = next)`. The success block
// is laid out AFTER `li r3,68 ; blr`, reached by a forward `beq-`, which is
// what an out-of-line return inside the loop gives.
//
// `cmplw cr6,r9,r4` puts the item's field in rA, so the test is written
// `p->key == key`.

#include "types.h"

struct Link
{
    /* 0x00 */ Link* next;
    /* 0x04 */ Link* prev;
};

struct ItemHead
{
    /* 0x00 */ u8 unk0000[92];
};

struct Item : public ItemHead, public Link
{
    /* 0x64 */ u8  unk0064[32];
    /* 0x84 */ u32 key;
};

ASSERT_OFFSET(Item, key, 132);

struct Registry
{
    /* 0x000 */ u8   unk0000[260];
    /* 0x104 */ Item head;
};

ASSERT_OFFSET(Registry, head, 260);

int RegistryFind(Registry* r, u32 key, Item** out)
{
    if (out == 0)
        return 37;

    *out = 0;

    Item* end = &r->head;

    for (Item* p = static_cast<Item*>(r->head.next); p != end;
         p = static_cast<Item*>(p->next))
    {
        if (p->key == key)
        {
            *out = p;
            return 0;
        }
    }

    return 68;
}
