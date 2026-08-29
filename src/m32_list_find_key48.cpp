// sub_82547880 -- the same routine as src/f6_list_find_key.cpp
// (sub_825478F8, 120 bytes later in the image) over a different list.
// 116 bytes, 4 callers.
//
//   cmplwi cr6,r5,0 ; bne- <body> ; li r3,37 ; blr
//   addi   r10,r3,412                      the SENTINEL, no null test
//   lwz    r11,460(r3)                     the head's own `next`
//   cmplwi cr6,r11,0 ; addi r11,r11,-48 ; bne- ; li r11,0
//   lwz    r9,76(r11)                      the key
//   lwz    r11,48(r11)                     the next
//   li     r3,68  /  stw r11,0(r5) ; li r3,0
//
// Every constant follows from one layout, which is what makes it the right
// one rather than merely a fitting one:
//
//   * the `-48` is the base-class adjustment run backwards, turning a link
//     pointer into the item that contains it, so `Link` sits at +48 of
//     `Item`. The null test around it exists only so a null link stays null.
//   * `addi r10,r3,412` carries NO null test, so the head is a whole `Item`
//     at +412 and not a bare `Link` -- MSVC cannot prove `r + 412` non-null
//     when the cast is a real upcast and emits three extra words, measured on
//     the sibling.
//   * 412 + 48 == 460, the word the walk starts from. One layout, both
//     constants.
//
// `return 37` is the FALL-THROUGH of the null test, so the guard is written
// first, and the success block sits after `li r3,68 ; blr`, reached by a
// forward `beq-` -- an out-of-line return from inside the loop.
//
// Nothing is relocated: 29 of 29 words are compared.

#include "types.h"

struct Link48
{
    /* 0x00 */ Link48* next;
    /* 0x04 */ Link48* prev;
};

struct ItemHead48
{
    /* 0x00 */ u8 unk0000[48];
};

struct Item48 : public ItemHead48, public Link48
{
    /* 0x38 */ u8  unk0038[20];
    /* 0x4C */ u32 key;
};

ASSERT_OFFSET(Item48, key, 76);

struct Table48
{
    /* 0x000 */ u8     unk0000[412];
    /* 0x19C */ Item48 head;
};

ASSERT_OFFSET(Table48, head, 412);

int TableFind(Table48* t, u32 key, Item48** out)
{
    if (out == 0)
        return 37;

    *out = 0;

    Item48* end = &t->head;

    for (Item48* p = static_cast<Item48*>(t->head.next); p != end;
         p = static_cast<Item48*>(p->next))
    {
        if (p->key == key)
        {
            *out = p;
            return 0;
        }
    }

    return 68;
}
