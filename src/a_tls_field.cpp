// sub_82602F98 -- 20 bytes, 31 callers. Address of a field of a
// thread-local object.
//
//      li      r11,20
//      lwz     r10,0(r13)
//      addi    r11,r11,8
//      add     r3,r11,r10
//      blr
//
// The tell is that 20 and 8 are NEVER FOLDED. A compiler that had two
// literal constants here would emit `addi r3,r10,28`; it emits an unfolded
// `li` plus an `addi` plus an `add` instead, which means the 20 is not a
// constant it is allowed to fold -- it is a link-time value. Combined with
// the load through r13, that is the Xbox 360 __declspec(thread) sequence:
// r13 points at the thread's TLS block pointer, the `li` carries the
// linker-assigned offset of the variable inside that block, and the `addi`
// is the ordinary field offset within the variable.

#include "types.h"

struct TlsBlock
{
    /* 0x00 */ char unk0000[0x08];
    /* 0x08 */ u32  field;
};

ASSERT_OFFSET(TlsBlock, field, 0x08);

__declspec(thread) extern TlsBlock g_tls;

u32* TlsField()
{
    return &g_tls.field;
}
