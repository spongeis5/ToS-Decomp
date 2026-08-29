// sub_82677028 -- clear a field, then tail-call with another. 20 B, 25 callers.
//
//      mr      r11,r3          keep this
//      lwz     r3,132(r3)      argument for the call
//      li      r10,0
//      stw     r10,136(r11)    this->f136 = 0
//      b       0x826E5210
//
// sub_82677040 is the same function against field 140 and a different
// callee -- a matched pair, so getting one right gets both.
//
// The load of f132 is emitted BEFORE the store to f136, which is why `this`
// has to survive in r11.

struct Owner
{
    char  pad0000[132];
    void* obj;                  // +0x84
    int   flag;                 // +0x88
};

void Handle(void*);

void ClearAndHandle(Owner* o)
{
    o->flag = 0;
    Handle(o->obj);
}
