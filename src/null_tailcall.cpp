// sub_826A3350 -- load, null-check, tail call. 16 bytes recorded, 19 callers.
//
//      lwz     r3,0(r3)
//      cmplwi  cr6,r3,0
//      beqlr   cr6
//      b       0x82662E08
//
// The loaded pointer becomes the argument, so the guard tests the value that
// is about to be passed. Ends in an unconditional branch, so the recorded
// size may be short by the unreachable trailing blr -- match.py reconciles.

struct Holder { void* obj; };

void Use(void*);

void UseIfPresent(Holder* h)
{
    void* p = h->obj;
    if (p)
        Use(p);
}
