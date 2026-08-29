// sub_821FE858 -- store the first argument, then tail-call slot 0 with the
// second. 24 B, 4 callers.
//
//   lwz   r11,0(r3)      the vtable, loaded BEFORE the store
//   stw   r4,4(r3)       this->owner = arg0
//   mr    r4,r5          arg1 becomes the callee's first argument
//   lwz   r10,0(r11)     slot 0/4 = 0
//   mtctr r10
//   bctr
//
// Register discipline: r3 is passed through UNCHANGED to the callee, so the
// object is its own `this` and this is a member function rather than a
// forwarder -- had the source formed a new receiver there would be an addi
// or a second load, and there is neither.
//
// The vtable load is emitted ahead of the store even though the call is
// written after it: `vt` and `owner` are two constant offsets off one base,
// which MSVC can prove do not alias, so it hoists the load to cover its own
// latency.  That is the documented hoist, not a source ordering -- writing
// the store first is what the source says and what this file does.

#include "types.h"

struct Sink;

struct SinkVT
{
    void (*Handle)(Sink* self, void* arg);
};

struct Sink
{
    /* 0x00 */ SinkVT* vt;
    /* 0x04 */ void*   owner;

    void Attach(void* o, void* arg);
};
ASSERT_OFFSET(Sink, vt,    0x00);
ASSERT_OFFSET(Sink, owner, 0x04);

void Sink::Attach(void* o, void* arg)
{
    owner = o;
    vt->Handle(this, arg);
}
