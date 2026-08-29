#include "types.h"

// sub_8262FE10 -- release with a 16-bit reference count. 60 B, 10 callers.
//
//      lhz     r11,4(r3)
//      cmplwi  cr6,r11,0
//      beqlr   cr6            not owned: done
//      lhz     r11,6(r3)
//      addi    r10,r11,-1
//      extsh   r9,r10         <- SIGNED 16-bit truncation
//      sth     r9,6(r3)       --this->m_ref
//      cmpwi   cr6,r9,0       <- signed test of the new value
//      bnelr   cr6            still referenced: done
//      lwz     r11,0(r3)
//      li      r4,1           <- the deleting-destructor flag
//      lwz     r10,0(r11)
//      mtctr   r10
//      bctr                   delete this
//
// `extsh` plus `cmpwi` is a SIGNED short; an unsigned short would clear the
// top with `rlwinm rX,rY,0,16,31` and test with `cmplwi`. The guard at +4 is
// read with `lhz`/`cmplwi`, so it is unsigned.
//
// The tail is the `delete this` idiom written up in src/m_release.cpp: a
// literal 1 in r4 and a call through vtable SLOT 0, which is where MSVC puts
// the scalar deleting destructor.
struct ShortRef
{
    /* 0x00 */ /* vptr */
    /* 0x04 */ u16   m_owned;
    /* 0x06 */ short m_ref;

    virtual ~ShortRef();
    void Release();
};

void ShortRef::Release()
{
    if (m_owned != 0)
    {
        if (--m_ref == 0)
            delete this;
    }
}
