// sub_8215D038 -- join a global directory string and a name into a caller's
// buffer with sprintf, and tail call. 32 bytes, 5 callers.
//
//      lis     r10,-32102
//      mr      r11,r4
//      lis     r9,-32256
//      mr      r6,r3
//      addi    r4,r9,12756         = 820031D4
//      lwz     r5,31236(r10)       = 829A7A04
//      mr      r3,r11
//      b       0x828A9CB8
//
// The three addresses, read out of the image:
//
//   820031D4   the 6-byte string  %s \ %s  (written "%s" backslash "%s"),
//              sitting between "Start" and "ControlOn" in .rdata
//   829A7A04   sixteen zero bytes -- an uninitialised global POINTER, read
//              with lis+lwz rather than lis+addi
//   828A9CB8   in the CRT band (828A74A0..82908510) and it opens
//              `std r5,32(r1) / std r6,40(r1) / ... / std r10,72(r1)`,
//              which is the varargs register save area, with r3 and r4 the
//              two fixed parameters and a null check on r4. That is
//              sprintf(char*, const char*, ...).
//
// So the FIRST parameter becomes the last %s and the SECOND parameter is the
// output buffer -- the argument order reads backwards and is worth stating.
//
// Four of the eight words are relocations (both halves of the string
// address, the global pointer load's lis, and the tail branch), so 4 of 8
// are compared.

#include "types.h"
#include <stdio.h>

extern const char* g_root;

int MakePath(const char* name, char* out)
{
    return sprintf(out, "%s\\%s", g_root, name);
}
