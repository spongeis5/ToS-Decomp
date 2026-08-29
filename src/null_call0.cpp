#include "types.h"

// sub_82603948 -- null guard, then a call with a zero argument.
// 20 B, 8 callers.
//   cmplwi cr6,r3,0 ; beqlr cr6 ; li r4,0 ; b 0x82602F08 ; blr
struct Obj0;
void Release(Obj0*, int);
void ReleaseIfAny(Obj0* o) { if (o) Release(o, 0); }
