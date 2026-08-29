// sub_82600BD8 -- read one field of a global object. 16 B, 135 callers.
//
//      lis     r11,-32092
//      addi    r10,r11,-29632
//      lwz     r3,152(r10)
//      blr
//
// The address is formed into r10 and the field offset stays in the load, so
// the source reads a member of a global rather than through a pointer.
// 2 of 4 words are relocated.

struct Global
{
    char pad0000[152];
    int  field;                 // +0x98
};

extern Global g_object;

int GetGlobalField()
{
    return g_object.field;
}
