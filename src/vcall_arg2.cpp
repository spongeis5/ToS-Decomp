// sub_82600BB0 -- virtual call on the SECOND argument, 20 bytes, 19 callers.
//
//      lwz     r11,0(r4)       vtable of arg2
//      mr      r3,r4           arg2 becomes the this pointer
//      lwz     r10,4(r11)      slot 1
//      mtctr   r10
//      bctr
//
// The first argument is loaded over and never used. Note that unlike
// sub_827C5198 the compiler picked a FRESH register (r10) for the slot here
// rather than reusing r11 -- so that choice is context-dependent, not a
// fixed habit, which is worth knowing given 827C5198 stalls on exactly it.

struct Target;
struct VTable { void* (*slot[2])(Target*); };
struct Target { VTable* vt; };

void* CallOnSecond(void*, Target* t)
{
    return t->vt->slot[1](t);
}
