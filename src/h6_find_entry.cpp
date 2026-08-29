#include "types.h"

// sub_822DD9F8 -- linear search of a 40-byte entry array, returning one of
// three pointers chosen by a parallel 56-byte info array and a mode argument.
// 228 B, 5 callers.
//
// Both strides are stated by the image, not guessed:
//
//   entries   addi r10,r10,40                 and the exit blocks rebuild it
//             rlwinm i,2 ; add ; rlwinm ,3    as (i + i*4) * 8 = i*40
//   info      addi r9,r9,56                   added to a base and read at +5
//
// so ASSERT_SIZE on both is measured. The `lbz r8,5(r8)` says the selector is
// a BYTE at +5 of the info element.
//
// SIGNEDNESS IS SPLIT ACROSS THE TWO KEYS, and the compares say so:
//   cmpw   cr6,r8,r4     key4 is SIGNED
//   cmplw  cr6,r8,r5     key0 is UNSIGNED
// Offset 4 is tested first, so it is written first.
//
// THE MODE TEST IS A SWITCH, not a chain of `==`. An if-chain over 0/1/2
// compares against 0, then 1, then 2; what the image holds is
// `cmplwi r6,1 / blt / beq / cmplwi r6,3 / bge`, which is MSVC's binary
// decision tree over the dense case set {0,1,2} with a default -- pivot at 1,
// then the remainder split at 3. Case 1 and the default return the same
// pointer as the kind==0 path, and all three share one block at 822DDA88.
//
// The kind test above it IS a chain of `==`: two separate `cmplwi` against 0
// and 1, with anything else falling through to the next iteration.

struct EntryRec
{
    /* 0x00 */ u32  key0;
    /* 0x04 */ s32  key4;
    /* 0x08 */ void* p08;
    /* 0x0C */ void* p0C;
    /* 0x10 */ void* p10;
    /* 0x14 */ char unk0014[0x14];
};
ASSERT_OFFSET(EntryRec, key0, 0x00);
ASSERT_OFFSET(EntryRec, key4, 0x04);
ASSERT_OFFSET(EntryRec, p08, 0x08);
ASSERT_OFFSET(EntryRec, p0C, 0x0C);
ASSERT_OFFSET(EntryRec, p10, 0x10);
ASSERT_SIZE(EntryRec, 40);

struct InfoRec
{
    /* 0x00 */ char unk0000[0x05];
    /* 0x05 */ u8   kind;
    /* 0x06 */ char unk0006[0x32];
};
ASSERT_OFFSET(InfoRec, kind, 0x05);
ASSERT_SIZE(InfoRec, 56);

struct InfoTable
{
    /* 0x00 */ s32      count;
    /* 0x04 */ InfoRec* info;
};
ASSERT_OFFSET(InfoTable, count, 0x00);
ASSERT_OFFSET(InfoTable, info, 0x04);

struct EntryOwner
{
    /* 0x00 */ char       unk0000[0x40];
    /* 0x40 */ InfoTable* table;
    /* 0x44 */ char       unk0044[0x04];
    /* 0x48 */ EntryRec*  entries;
};
ASSERT_OFFSET(EntryOwner, table, 0x40);
ASSERT_OFFSET(EntryOwner, entries, 0x48);

void* FindEntry(EntryOwner* o, s32 key4, u32 key0, u32 mode)
{
    InfoTable* t = o->table;
    s32 n = t->count;

    for (s32 i = 0; i < n; i++)
    {
        if (o->entries[i].key4 == key4 && o->entries[i].key0 == key0)
        {
            u32 kind = t->info[i].kind;

            if (kind == 0)
                return o->entries[i].p08;

            if (kind == 1)
            {
                switch (mode)
                {
                case 0:  return o->entries[i].p0C;
                case 1:  return o->entries[i].p08;
                case 2:  return o->entries[i].p10;
                default: return o->entries[i].p08;
                }
            }
        }
    }

    return 0;
}
