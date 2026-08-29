// sub_82542518 -- publish a configuration block into a global the first time
// it is asked for, filling in defaults it has not got. 124 B, 4 callers.
//
//      lis  r11,-32098 ; addi r10,r11,-28864        -> 829D8F40, the global
//      lwz  r11,456(r10) ; cmplwi cr6,r11,0 ; bne- cr6,<ret0>
//      lis  r11,-32105 ; lwz r11,-14932(r11)        -> 8296C5AC, a POINTER
//      addi r11,r11,56
//      stw  r11,456(r10)
//      lwz  r9,4(r11) ; cmplwi cr6,r9,0 ; bne- cr6,<ret0>
//      lwz  r8,0(r11)
//      li   r9,4 ; li r7,-1 ; lis r6,32
//      stw  r9,20(r11) ; stw r7,12(r11)
//      cmplwi cr6,r8,0 ; stw r6,16(r11) ; bne- cr6,<rest>
//      lis  r8,22616 ; stw r9,436(r10) ; ori r7,r8,22616 ; stw r7,0(r11)
//  rest:
//      li r10,4096 ; lis r9,1 ; stw r10,4(r11) ; stw r9,8(r11)
//  ret0:
//      li r3,0 ; blr
//
// TWO different globals, and the difference between them is readable. The
// first is an OBJECT: `lis` + `addi` materialises its address and the field
// offsets stay in the loads, exactly as in src/global_field.cpp. The second
// is a POINTER VARIABLE: its low half folds into the `lwz` itself, which only
// happens at offset 0, and the `addi ...,56` afterwards is then a member of
// what it points at -- a plain address-of with no null test.
//
// Every immediate is built the way MSVC builds one that does not fit in 16
// bits: `lis r6,32` is 0x00200000, `lis r9,1` is 0x00010000, and
// `lis r8,22616 ; ori r7,r8,22616` is 0x58585858. The `li r9,4` is reused
// for two unrelated stores, which is why the 4 at +436 and the 4 at +20 share
// a register.
//
// All three tests are `== 0` with the body as the FALL-THROUGH of a `bne-`,
// so all three are written as positive guards around nested work rather than
// as early returns.
//
// Store order is source order: 20, 12, 16, then the conditional pair, then
// 4 and 8.
//
// 4 of 31 words are relocated.

#include "types.h"

struct Config
{
    /* 0x00 */ u32 magic;
    /* 0x04 */ u32 size;
    /* 0x08 */ u32 version;
    /* 0x0C */ s32 owner;
    /* 0x10 */ u32 limit;
    /* 0x14 */ u32 align;
};

ASSERT_OFFSET(Config, owner, 0x0C);
ASSERT_OFFSET(Config, align, 0x14);

struct Host
{
    /* 0x00 */ u8     unk0000[56];
    /* 0x38 */ Config config;
};

ASSERT_OFFSET(Host, config, 56);

struct Globals
{
    /* 0x000 */ u8      unk0000[436];
    /* 0x1B4 */ u32     align;
    /* 0x1B8 */ u8      unk01B8[16];
    /* 0x1C8 */ Config* config;
};

ASSERT_OFFSET(Globals, align, 436);
ASSERT_OFFSET(Globals, config, 456);

extern Globals g_globals;
extern Host*   g_host;

int EnsureConfig()
{
    if (g_globals.config == 0)
    {
        Config* c = &g_host->config;
        g_globals.config = c;

        if (c->size == 0)
        {
            c->align = 4;
            c->owner = -1;
            c->limit = 0x00200000;

            if (c->magic == 0)
            {
                g_globals.align = 4;
                c->magic = 0x58585858;
            }

            c->size = 4096;
            c->version = 0x00010000;
        }
    }

    return 0;
}
