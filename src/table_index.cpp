// sub_822D2450 -- address of a field inside a global array element,
// 24 bytes, 59 callers.
//
//      lis     r10,-32099
//      mulli   r11,r3,1856         element stride
//      addi    r10,r10,-21312      \  the symbol's low half
//      addi    r10,r10,1248        /  the FIELD offset, kept separate
//      add     r3,r11,r10
//      blr
//
// The two consecutive addi are the tell. MSVC emits lis/addi as a relocated
// pair for the symbol itself and cannot fold a constant into the second half,
// so a field offset inside the element becomes its own addi. That means the
// source takes the address of a MEMBER, not of the element:
//
//      &g_table[i].field           not     &g_table[i]
//
// mulli rather than a shift sequence means the stride is not a power of two:
// sizeof(Entry) must be exactly 1856.
//
// The lis and the first addi are relocated, so 4 of 6 words are compared.

struct Entry
{
    char pad0000[1248];
    int  field;                 // +0x4E0
    char pad04E4[1856 - 1248 - 4];
};

extern Entry g_table[];

int* FieldOf(int i)
{
    return &g_table[i].field;
}
