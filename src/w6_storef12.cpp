#include "types.h"

// sub_8224EA08 -- store the float argument into a global. 12 B, bridge
// between 8224E9F0 and 8224EA18.
//
//      lis     r11,-32107
//      stfs    f1,25388(r11)     -> 8295632C
//      blr

extern float g_float_8295632C;

void StoreFloat(float v)
{
    g_float_8295632C = v;
}
