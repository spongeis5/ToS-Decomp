#include "types.h"

// sub_8253FE28 -- zero 48 words with a counted loop. 28 B, 9 callers.
//   li r10,48 ; addi r11,r3,-4 ; li r9,0 ; mtctr r10
//   stwu r9,4(r11) ; bdnz+ 0x8253FE38 ; blr
//
// mtctr/bdnz is the counted-loop form, and the pointer is biased by -4 so
// the update-form store can do the increment and the write in one
// instruction -- the same trick as the byte-copy loop in string_utils.cpp.
void Zero48(int* p)
{
    for (int i = 0; i < 48; i++)
        p[i] = 0;
}
