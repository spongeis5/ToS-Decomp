#include "types.h"

// sub_82697608 -- guard on the THIRD argument. 16 B, 6 callers.
//   cmpwi cr6,r5,0 ; beqlr cr6 ; b 0x828A9968 ; blr
// cmpwi is signed, so the parameter is a signed int.
void Forward3(void*, void*, int);
void Forward3If(void* a, void* b, int n) { if (n) Forward3(a, b, n); }
