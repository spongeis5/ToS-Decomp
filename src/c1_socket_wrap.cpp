// sub_825FA780 -- the game's socket() wrapper: shift three arguments up one
// slot, prepend the caller type, and tail call the xam.xex import.
// 20 bytes, 5 callers.
//
//      mr      r6,r5
//      mr      r5,r4
//      mr      r4,r3
//      li      r3,1
//      b       0x8291303C
//
// 8291303C is NOT a function in .text -- it is an XEX import thunk
// (`.long 0x01000003 / .long 0x02000003 / mtctr r11 / bctr`), and
// build/imports.txt names it `xam.xex ordinal 3 NetDll_socket`.
//
// The Xbox 360 NetDll entry points all take an XNCALLER_TYPE as their first
// argument ahead of the Winsock signature, and the literal here is 1 =
// XNCALLER_TITLE. So this is the one-line adapter between the title's own
// `socket(af, type, protocol)` and the import.
//
// The three shifts are `mr`, not `rlwinm rD,rS,0,0,31`, so the forwarded
// arguments are signed or pointer typed -- plain `int` here.
//
// The trailing `b` is a relocation, so 4 of the 5 words are compared.

#include "types.h"

int NetDll_socket(int caller, int af, int type, int protocol);

int Socket(int af, int type, int protocol)
{
    return NetDll_socket(1, af, type, protocol);
}
