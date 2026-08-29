// sub_827C5198 -- virtual call through a member object, 20 bytes, 48 callers.
//
//      lwz     r3,116(r3)      r3 = this->member
//      lwz     r11,0(r3)       vtable
//      lwz     r11,64(r11)     slot 64/4 = 16
//      mtctr   r11
//      bctr                    tail call, so the result is returned directly
//
// Written with an explicit vtable rather than C++ `virtual`, because the slot
// index has to come out as exactly 16 and a real class would need seventeen
// declared virtual functions to get there. What is being matched is the
// generated code, and this generates it.
//
// Fully dependent: every instruction consumes the previous one's result.

struct Target;
struct VTable { void* (*slot[17])(Target*); };
struct Target { VTable* vt; };

struct Owner
{
    char    pad00[116];
    Target* member;             // +0x74
};

void* CallSlot16(Owner* o)
{
    Target* t = o->member;
    return t->vt->slot[16](t);
}
