// sub_8225FAC0 -- is the subsystem enabled and this job both counted and
// buffered? 56 B, 5 callers.
//
//      lis     r11,-32101
//      addi    r10,r11,-19728     -> 829AB2F0, a global OBJECT
//      lbz     r9,991(r10)        +0x3DF
//      cmplwi  cr6,r9,0
//      bne-    cr6,0x8225FAF0
//      lwz     r11,16(r3)         +0x10
//      cmpwi   cr6,r11,0          SIGNED -- an int field
//      beq-    cr6,0x8225FAF0
//      lwz     r11,8(r3)          +0x08
//      li      r3,1
//      cmplwi  cr6,r11,0          UNSIGNED -- a pointer field
//      bnelr   cr6
//  8225FAF0:
//      li      r3,0
//      blr
//
// `lis`+`addi` and then a displacement in the load is the ADDRESS of a global
// object, not a global pointer variable -- src/b_fwd_global5.cpp is the
// contrasting form. It is the same object src/t5_magic_check.cpp reads at
// +0x43C, so the name and type are taken from there and this file adds the
// byte at +0x3DF rather than inventing a second name for one address.
//
// The two compares differ in SIGNEDNESS on purpose: `cmpwi` on +0x10 says an
// int, `cmplwi` on +0x08 says a pointer. MATCHED.md's note that MSVC reuses
// cr6 from a signed compare for a following `!= 0` does not apply here --
// these are two different registers loaded from two different fields.
//
// All three guards branch FORWARD to ONE shared `li r3,0 ; blr` written last,
// which is the nested/`&&` spelling. Three flat `if (x) return 0;` guards
// plant a private zero return after the FIRST test instead (MATCHED.md,
// sub_821675B8, 2 of 14).
//
// There is NO trailing `clrlwi ...,24`, so the return type is NOT `bool`: the
// 0 and the 1 are computed straight into r3 and the function stops. `int`.
//
// 2 of 14 words are relocated (the lis/addi pair).

#include "types.h"

struct Stream;

struct StreamManager
{
    /* 0x000 */ char    unk0000[0x3DF];
    /* 0x3DF */ u8      disabled;
    /* 0x3E0 */ char    unk03E0[0x5C];
    /* 0x43C */ Stream* stream;
};

ASSERT_OFFSET(StreamManager, disabled, 0x3DF);
ASSERT_OFFSET(StreamManager, stream,   0x43C);

extern StreamManager g_streamManager;

struct Job
{
    /* 0x00 */ char  unk0000[0x08];
    /* 0x08 */ void* buffer;
    /* 0x0C */ char  unk000C[0x04];
    /* 0x10 */ s32   count;
};

ASSERT_OFFSET(Job, buffer, 0x08);
ASSERT_OFFSET(Job, count,  0x10);

int JobReady(Job* j)
{
    if (!g_streamManager.disabled && j->count != 0 && j->buffer != 0)
        return 1;
    return 0;
}
