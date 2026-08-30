#include "types.h"

// sub_82252F20 -- five-argument tail call with a magic and a name.
// 36 B, bridge between 82252EF8 and 82252F48.
//
//      lis r11,-32255 ; addi r5,r11,-6132   = 8200E80C "Rich Presence Manager"
//      lis r10,-32101 ; addi r3,r10,9508    = 829B2524, a global object
//      lis r4,21072 ; ori r4,r4,21069       = 0x5250524D "RPRM"
//      li r7,1 ; li r6,1
//      b 821FADB8
//
// r4's constant is built by lis+ori because the low half is >= 0x8000 --
// MSVC never folds a set-high-bit immediate into addi.

struct PresenceCtx
{
    char unk0000[64];
};

extern PresenceCtx g_presence_829B2524;
extern char kStr_8200E80C[];

int Tail_821FADB8(PresenceCtx*, unsigned int, char*, int, int);

void RichPresence()
{
    Tail_821FADB8(&g_presence_829B2524, 0x5250524Du, kStr_8200E80C, 1, 1);
}
