// sub_82677040 -- the twin of sub_82677028: same shape, field 140 instead of
// 136, and a different callee. 20 bytes, 25 callers.
//
//      mr      r11,r3
//      lwz     r3,132(r3)
//      li      r10,0
//      stw     r10,140(r11)
//      b       0x826E50D0

struct Owner
{
    char  pad0000[132];
    void* obj;                  // +0x84
    int   pad0088;              // +0x88
    int   flag;                 // +0x8C
};

void HandleOther(void*);

void ClearAndHandleOther(Owner* o)
{
    o->flag = 0;
    HandleOther(o->obj);
}
